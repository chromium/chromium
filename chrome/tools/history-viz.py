#!/usr/bin/env python
# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Process a History database and dump a .dot file suitable for GraphViz.

This is useful for debugging history redirect flows.

An example run of this program:
  python /src/history-viz.py History > foo.dot
  /c/Program\ Files/Graphviz2.18/bin/dot -Tpng foo.dot -o foo.png
"""

import struct
import subprocess
import sys
import urlparse


# Some transition types, copied from page_transition_types.h.
TRANS_TYPES = {
  0: 'link',
  1: 'typed',
  2: 'most-visited',
  3: 'auto subframe',
  7: 'form',
}


class URL(object):
  """Represents a broken-down URL from our most visited database."""

  def __init__(self, id, url):
    """Initialize a new URL object.  |id| is the database id of the URL."""
    self.id = id
    self.url = url
    scheme, loc, path, query, fragment = urlparse.urlsplit(url)
    if scheme == 'http':
      scheme = ''  # Shorten for display purposes.
    if len(scheme) > 0:
      scheme += '://'
    self.host = scheme + loc
    self.path = path

    extra = ''
    if len(query) > 0:
      extra += '?' + query
    if len(fragment) > 0 or url.find('#') > 0:
      extra += '#' + fragment
    self.extra = extra

  def PrettyPrint(self, include_host=True, include_path=True):
    """Pretty-print this URL in a form more suitable for the graph.

    This will elide very long paths and potentially puts newlines between parts
    of long components.  include_host and include_path determine whether to
    include the host and path in the output.

    Returns: the pretty-printed string."""
    MAX_LEN = 30  # Maximum length of a line in the output.
    parts = []
    if include_host:
      parts.append(self.host)
    if include_path:
      parts.append(self.path)
    parts.append(self.extra)
    lines = []
    line = ''
    for part in parts:
      if len(part) > MAX_LEN:
        part = part[0:MAX_LEN-3] + '...'
      if len(line)+len(part) > MAX_LEN:
        lines.append(line)
        line = ''
      line += part
    if len(line) > 0:
      lines.append(line)
    return '\n'.join(lines)


class Edge(object):
  """Represents an edge in the history graph, connecting two pages.

  If a link is traversed twice, it is one Edge with two entries in
  the .transitions array."""
  def __init__(self, src, dst):
    self.src = src
    self.dst = dst
    self.transitions = []

  def Transitions(self):
    """Return a dictionary mapping transition type -> occurences."""
    all = {}
    for trans in self.transitions:
      all[trans] = all.get(trans, 0) + 1
      # We currently don't use the chain type.
      # TODO(evanm): make this a command-line option.
      # if trans & 0x30000000 != 0:
      #   chain = ''
      #   if trans & 0x10000000:
      #     chain = 'start'
      #   if trans & 0x20000000:
      #     if len(chain) == 0:
      #       chain = 'end'
      #     else:
      #       chain = ''
      #   if len(chain) > 0:
      #     edge['chain'] = chain
    return all


def ClusterBy(objs, pred):
  """Group a list of objects by a predicate.

  Given a list of objects and a predicate over the objects, return a
  dictionary mapping pred(obj) -> all objs with the same pred(obj)."""
  clusters = {}
  for obj in objs:
    cluster = pred(obj)
    clusters[cluster] = clusters.get(cluster, [])
    clusters[cluster].append(obj)
  return clusters


def EscapeDot(string):
  """Escape a string suitable for embedding in a graphviz graph."""
  # TODO(evanm): this is likely not sufficient.
  return string.replace('\n', '\\n')


class SQLite(object):
  """Trivial interface to executing SQLite queries.
  Spawns a new process with each call."""
  def __init__(self, file=None):
    self.file = file

  def Run(self, sql):
    """Execute |sql|, yielding each row of results as an array."""
    subproc = subprocess.Popen(['sqlite', self.file],
                               stdin=subprocess.PIPE,
                               stdout=subprocess.PIPE)
    subproc.stdin.write('.mode tabs\n')
    subproc.stdin.write(sql + ';')
    subproc.stdin.close()
    for line in subproc.stdout:
      row = line.strip().split('\t')
      yield row


def LoadHistory(filename):
  db = SQLite(filename)

  urls = {}  # Map of urlid => url.
  urls['0'] = URL('0', 'start')  # Node name '0' is our special 'start' node.
  for id, url in db.Run('SELECT id, url FROM urls'):
    urls[id] = URL(id, url)

  visiturlids = {}  # Map of visitid => urlid.
  visiturlids['0'] = '0'  # '0' is our special 'start' node.
  edges = {}  # Map of urlid->urlid->Edge.
  for src, dst, url, trans in db.Run('SELECT from_visit, id, url, transition '
                                     'FROM visits ORDER BY id'):
    visiturlids[dst] = url
    src = visiturlids[src]
    dst = visiturlids[dst]
    edges[src] = edges.get(src, {})
    edge = edges[src][dst] = edges[src].get(dst, Edge(src, dst))
    # SQLite outputs transition values as signed integers, but they're really
    # a bitfield.  Below does "unsigned trans = static_cast<unsigned>(trans)".
    trans = struct.unpack('I', struct.pack('i', int(trans)))[0]
    edge.transitions.append(trans)

  return urls, edges


def main():
  urls, edges = LoadHistory(sys.argv[1])
  print 'digraph G {'
  print '  graph [rankdir=LR]'  # Display left to right.
  print '  node [shape=box]'    # Display nodes as boxes.
  print '  subgraph { rank=source; 0 [label="start"] }'

  # Output all the nodes within graph clusters.
  hosts = ClusterBy(urls.values(), lambda url: url.host)
  for i, (host, urls) in enumerate(hosts.items()):
    # Cluster all URLs under this host if it has more than one entry.
    host_clustered = len(urls) > 1
    if host_clustered:
      print 'subgraph clusterhost%d {' % i
      print '  label="%s"' % host
    paths = ClusterBy(urls, lambda url: url.path)
    for j, (path, urls) in enumerate(paths.items()):
      # Cluster all URLs under this host if it has more than one entry.
      path_clustered = host_clustered and len(urls) > 1
      if path_clustered:
        print '  subgraph cluster%d%d {' % (i, j)
        print '    label="%s"' % path
      for url in urls:
        if url.id == '0': continue  # We already output the special start node.
        pretty = url.PrettyPrint(include_host=not host_clustered,
                                include_path=not path_clustered)
        print '    %s [label="%s"]' % (url.id, EscapeDot(pretty))
      if path_clustered:
        print '  }'
    if host_clustered:
      print '}'

  # Output all the edges between nodes.
  for src, dsts in edges.items():
    for dst, edge in dsts.items():
      # Gather up all the transitions into the label.
      label = []      # Label for the edge.
      transitions = edge.Transitions()
      for trans, count in transitions.items():
        text = ''
        if count > 1:
          text = '%dx ' % count
        base_type = trans & 0xFF
        redir = (trans & 0xC0000000) != 0
        start = (trans & 0x10000000) != 0
        end = (trans & 0x20000000) != 0
        if start or end:
          if start:
            text += '<'
          if end:
            text += '>'
          text += ' '
        if redir:
          text += 'R '
        text += TRANS_TYPES.get(base_type, 'trans%d' % base_type)
        label.append(text)
      if len(label) == 0:
        continue

      edgeattrs = []  # Graphviz attributes for the edge.
      # If the edge is from the start and the transitions are fishy, make it
      # display as a dotted line.
      if src == '0' and len(transitions.keys()) == 1 and 0 in transitions:
        edgeattrs.append('style=dashed')
      if len(label) > 0:
        edgeattrs.append('label="%s"' % EscapeDot('\n'.join(label)))

      out = '%s -> %s' % (src, dst)
      if len(edgeattrs) > 0:
        out += ' [%s]' % ','.join(edgeattrs)
      print out
  print '}'
  return 0


if __name__ == '__main__':
  sys.exit(main())
