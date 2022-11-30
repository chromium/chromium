// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_DELEGATE_H_
#define COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_DELEGATE_H_

#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/files/file_path.h"
#include "components/file_access/scoped_file_access.h"
#include "url/gurl.h"

class ScopedFileAccessDelegateTest;

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

 protected:
  ScopedFileAccessDelegate();

  virtual ~ScopedFileAccessDelegate();

  // A single instance of ScopedFileAccessDelegate. Equals nullptr when there's
  // not any data transfer restrictions required.
  static ScopedFileAccessDelegate* scoped_file_access_delegate_;

  friend class ::ScopedFileAccessDelegateTest;
};
}  // namespace file_access

#endif  // COMPONENTS_FILE_ACCESS_SCOPED_FILE_ACCESS_DELEGATE_H_
