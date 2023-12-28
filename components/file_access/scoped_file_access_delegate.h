// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_DELEGATE_H_
#define COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_DELEGATE_H_

#include <memory>
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
// instance. When it is replaced the old instance is destructed. This instance
// is constructed and destructed on the UI thread. So all methods should only be
// called from the UI thread. The exception is RequestDefaultFilesAccessIO,
// which takes care of hopping correctly between the threads and providing this
// to packages without direct access to the UI thread.
class COMPONENT_EXPORT(FILE_ACCESS) ScopedFileAccessDelegate {
 public:
  using RequestFilesAccessIOCallback =
      base::RepeatingCallback<void(const std::vector<base::FilePath>&,
                                   base::OnceCallback<void(ScopedFileAccess)>)>;

  using RequestFilesAccessCheckDefaultCallback =
      base::RepeatingCallback<void(const std::vector<base::FilePath>&,
                                   base::OnceCallback<void(ScopedFileAccess)>,
                                   bool check_default)>;
  // When new entries are added, EnterpriseDlpFilesDefaultAccess enum in
  // histograms/enums.xml should be updated.
  enum class DefaultAccess {
    kMyFilesAllow = 0,
    kMyFilesDeny = 1,
    kSystemFilesAllow = 2,
    kSystemFilesDeny = 3,
    kMaxValue = kSystemFilesDeny
  };

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

  // If feature DataControlsDefaultDeny is not set, requests access to |files|.
  // |callback| is called with a token that should be hold until `open()`
  // operation on the files finished. If the feature is set the `callback` is
  // called with a dummy `ScopedFileAccess`, which will lead the daemon to deny
  // access, if the file is restricted.
  virtual void RequestDefaultFilesAccess(
      const std::vector<base::FilePath>& files,
      base::OnceCallback<void(file_access::ScopedFileAccess)> callback) = 0;

  // Creates a callback to gain file access for the given `destination`. The
  // callback should be called on the IO thread. The method itself from the UI
  // thread.
  virtual RequestFilesAccessIOCallback CreateFileAccessCallback(
      const GURL& destination) const = 0;

  // Called from the IO thread. Depending on the default behaviour
  // (kDataControlsDefaultDeny feature) executes the callback without requesting
  // file access (denying access to managed files) or switch to the UI thread
  // and call RequestFilesAccessForSystem there. The `callback` is run on the IO
  // thread in both cases. The feature might get removed at some point
  // (b/306619855); the behaviour after that will like executing the `callback`
  // directly.
  static void RequestDefaultFilesAccessIO(
      const std::vector<base::FilePath>& files,
      base::OnceCallback<void(ScopedFileAccess)> callback);

  // Called from the IO thread. The method will switch to the UI thread and call
  // RequestFilesAccessForSystem there. The `callback` is run on the IO thread
  // with the gained file access token.
  static void RequestFilesAccessForSystemIO(
      const std::vector<base::FilePath>& files,
      base::OnceCallback<void(ScopedFileAccess)> callback);

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

  // This class sets the callback forwarding the RequestFilesAccessForSystem
  // call from IO to UI thread.
  class COMPONENT_EXPORT(FILE_ACCESS)
      ScopedRequestFilesAccessCallbackForTesting {
   public:
    // If `restore_original_callback` is set, it restores the original callback.
    // Otherwise, it destroys the original callback when this class is
    // destroyed.
    explicit ScopedRequestFilesAccessCallbackForTesting(
        RequestFilesAccessIOCallback callback,
        bool restore_original_callback = true);

    virtual ~ScopedRequestFilesAccessCallbackForTesting();

    ScopedRequestFilesAccessCallbackForTesting(
        const ScopedRequestFilesAccessCallbackForTesting&) = delete;
    ScopedRequestFilesAccessCallbackForTesting& operator=(
        const ScopedRequestFilesAccessCallbackForTesting&) = delete;

    void RunOriginalCallback(
        const std::vector<base::FilePath>& path,
        base::OnceCallback<void(file_access::ScopedFileAccess)> callback);

   private:
    bool restore_original_callback_;
    std::unique_ptr<RequestFilesAccessCheckDefaultCallback> original_callback_ =
        nullptr;
  };
  // Get a callback to get file access to files for system component
  // destination. Can be called from IO or UI thread. The callback should be
  // called on IO thread only.
  static RequestFilesAccessIOCallback GetCallbackForSystem();

 protected:
  ScopedFileAccessDelegate();

  virtual ~ScopedFileAccessDelegate();

  // A single instance of ScopedFileAccessDelegate. Equals nullptr when there's
  // not any data transfer restrictions required.
  static ScopedFileAccessDelegate* scoped_file_access_delegate_;

  // A single instance for a callback living on the IO thread which switches to
  // the UI thread to call RequestFilesAccessForSystem from there and switch
  // back to IO thread handing the ScopedFileAccess to another (given) callback.
  static RequestFilesAccessCheckDefaultCallback*
      request_files_access_for_system_io_callback_;
};

// Calls ScopedFilesAccessDelegate::RequestFilesAccess if
// ScopedFilesAccessDelegate::HasInstance returns true, immediately calls the
// callback with a ScopedFileAccess::Allowed object otherwise.
COMPONENT_EXPORT(FILE_ACCESS)
void RequestFilesAccess(
    const std::vector<base::FilePath>& files,
    const GURL& destination_url,
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback);

// Calls ScopedFilesAccessDelegate::RequestFilesAccessForSystem if
// ScopedFilesAccessDelegate::HasInstance returns true, immediately calls the
// callback with a ScopedFileAccess::Allowed object otherwise.
COMPONENT_EXPORT(FILE_ACCESS)
void RequestFilesAccessForSystem(
    const std::vector<base::FilePath>& files,
    base::OnceCallback<void(file_access::ScopedFileAccess)> callback);

// Calls ScopedFilesAccessDelegate::CreateFileAccessCallback if
// ScopedFilesAccessDelegate::HasInstance returns true, returns a callback with
// a ScopedFileAccess::Allowed object otherwise.
COMPONENT_EXPORT(FILE_ACCESS)
ScopedFileAccessDelegate::RequestFilesAccessIOCallback CreateFileAccessCallback(
    const GURL& destination);

}  // namespace file_access

#endif  // COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_DELEGATE_H_
