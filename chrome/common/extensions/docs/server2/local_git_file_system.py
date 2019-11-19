# Copyright 2015 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


import local_git_util
import posixpath

from environment import IsTest, IsAppEngine
from file_system import FileNotFoundError, FileSystem, StatInfo
from future import Future
from local_file_system import LocalFileSystem
from path_util import IsDirectory


class LocalGitFileSystem(FileSystem):
  '''Class to fetch filesystem data from this script's local git repository.
  '''
  @classmethod
  def Create(cls, branch='master', commit=None):
    if IsTest():
      return LocalFileSystem.Create('')
    return LocalGitFileSystem(branch, commit)

  def __init__(self, branch, pinned_commit):
    self._branch = branch
    self._pinned_commit = pinned_commit
    if self._pinned_commit:
      commit = self._pinned_commit
    else:
      if branch != 'master':
        commit = 'branch-heads/%s' % branch
      else:
        commit = 'origin/master'
    # Don't bother trying to load git stuff from AppEngine; it won't work.
    if not IsAppEngine():
      try:
        self._commit = local_git_util.ParseRevision(commit)
      except ImportError:
        # TODO(devlin|karandeepb): The comment below is old, and we can probably
        # remove this try-except block. But let's tackle that when there are no
        # fires.
        # We ignore ImportErrors here. It means we're running in AppEngine, so
        # this doesn't need to work anyway.
        pass

  def Read(self, paths, skip_not_found=False):

    def get_entry_name(entry):
      if entry['type'] == 'tree':
        return entry['name'] + '/'
      return entry['name']

    def read_path(path):
      try:
        if IsDirectory(path):
          return [get_entry_name(e)
                  for e in local_git_util.ListDir(path, self._commit)]
        else:
          return local_git_util.ReadFile(path, self._commit)
      except FileNotFoundError as e:
        if skip_not_found:
          return None
        raise e

    results = dict((path, read_path(path)) for path in paths)
    return Future(value=dict((k, v) for k, v in results.iteritems()
                             if v is not None))

  def Refresh(self):
    return Future(value=())

  def GetCommitID(self):
    '''Returns a future that resolves to the commit ID for this file system's
    revision.
    '''
    return Future(value=self._commit)

  def GetPreviousCommitID(self):
    '''Returns a future that resolves to the parent commit ID of this file
    system's revision.
    '''
    return Future(value=local_git_util.GetParentRevision(self._commit))

  def StatAsync(self, path):

    def get_child_versions(path):
      return dict((e['name'], e['id'])
                  for e in local_git_util.ListDir(path, self._commit))

    def get_file_version(dir, filename):
      try:
        return next(e['id'] for e in local_git_util.ListDir(dir, self._commit)
                    if e['name'] == filename)
      except StopIteration:
        raise FileNotFoundError('%s not found in revision %s' %
                                (path, self._commit))

    dir, filename = posixpath.split(path)
    if path == '':
      version = local_git_util.GetRootTree(self._commit)
      child_versions = get_child_versions('')
    elif IsDirectory(path):
      parent_dir, stat_dir = posixpath.split(dir)
      version = get_file_version(parent_dir, stat_dir)
      child_versions = get_child_versions(dir)
    else:
      version = get_file_version(dir, filename)
      child_versions = None

    #print 'Accessing local git for stat on %s (%s)' % (path, version)
    return Future(value=StatInfo(version, child_versions))

  def GetIdentity(self):
    if self._branch == 'master':
      # A master FS always carries the same identity even if pinned to a commit.
      str_id = 'master'
    elif self._pinned_commit is not None:
      str_id = self._pinned_commit
    else:
      str_id = 'branch-heads/%s' % self._branch
    return '@'.join((self.__class__.__name__, str_id))

  def GetVersion(self):
    return self._pinned_commit
