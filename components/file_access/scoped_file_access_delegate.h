// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_DELEGATE_H_
#define COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_DELEGATE_H_

#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "components/file_access/scoped_file_access.h"
#include "url/gurl.h"

namespace file_access {

// This class is mainly a interface and used to delegate DLP checks to
// appropriate proxy. It is used for managed ChromeOs only in the implementation
// DlpScopedfileAccessDelegate. Only one instance of a class which extends
// this class can exist at a time. The class itself also manages this one
// instance. When it is replaced the old instance is destructed.
class COMPONENT_EXPORT(FILE_ACCESS) ScopedFileAccessDelegate {
 public:
  ScopedFileAccessDelegate(const ScopedFileAccessDelegate&) = delete;
  ScopedFileAccessDelegate& operator=(const ScopedFileAccessDelegate&) = delete;

  // Returns a pointer to the existing instance of the class.
  static ScopedFileAccessDelegate* Get();

  // Returns true if an instance exists, without forcing an initialization.
  static bool HasInstance();

  // Deletes the existing instance of the class if it's already created.
  // Indicates that restricting data transfer is no longer required.
  // The instance will be deconstructed
  static void DeleteInstance();

  // Requests access to |files| in order to be sent to |destination_url|.
  // |callback| is called with a token that should be hold until
  // `open()` operation on the files finished.
  virtual void RequestFilesAccess(
      const std::vector<base::FilePath>& files,
      const GURL& destination_url,
      base::OnceCallback<void(file_access::ScopedFileAccess)> callback) = 0;

  // Requests access to |files| in order to be sent to a system process.
  // |callback| is called with a token that should be hold until
  // `open()` operation on the files finished.
  virtual void RequestFilesAccessForSystem(
      const std::vector<base::FilePath>& files,
      base::OnceCallback<void(file_access::ScopedFileAccess)> callback) = 0;

  // Calls base::ThreadPool::PostTaskAndReplyWithResult but `task` is run with
  // file access to `path`. The file access is hold until the call to `reply`
  // returns.
  template <typename T>
  void AccessScopedPostTaskAndReplyWithResult(
      const base::FilePath& path,
      const base::Location& from_here,
      const base::TaskTraits& traits,
      base::OnceCallback<T()> task,
      base::OnceCallback<void(T)> reply) {
    file_access::ScopedFileAccessDelegate::Get()->RequestFilesAccessForSystem(
        {path},
        base::BindOnce(
            [](const base::FilePath path, const base::Location& from_here,
               const base::TaskTraits traits, base::OnceCallback<T()> task,
               base::OnceCallback<void(T)> reply,
               file_access::ScopedFileAccess file_access) {
              base::ThreadPool::PostTaskAndReplyWithResult(
                  from_here, traits, std::move(task),
                  base::BindOnce([](base::OnceCallback<void(T)> reply,
                                    file_access::ScopedFileAccess file_access,
                                    T arg) { std::move(reply).Run(arg); },
                                 std::move(reply), std::move(file_access)));
            },
            path, from_here, traits, std::move(task), std::move(reply)));
  }

 protected:
  ScopedFileAccessDelegate();

  virtual ~ScopedFileAccessDelegate();

  // A single instance of ScopedFileAccessDelegate. Equals nullptr when there's
  // not any data transfer restrictions required.
  static ScopedFileAccessDelegate* scoped_file_access_delegate_;
};

}  // namespace file_access

#endif  // COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_DELEGATE_H_
