// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_DLP_DLP_CLIENT_H_
#define CHROMEOS_DBUS_DLP_DLP_CLIENT_H_

#include <string>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "dbus/object_proxy.h"

namespace dbus {
class Bus;
}  // namespace dbus

namespace chromeos {

// DlpClient is used to communicate with the org.chromium.Dlp
// service. All method should be called from the origin thread (UI thread) which
// initializes the DBusThreadManager instance.
class COMPONENT_EXPORT(DLP) DlpClient {
 public:
  using SetDlpFilesPolicyCallback =
      base::OnceCallback<void(const dlp::SetDlpFilesPolicyResponse response)>;
  using AddFileCallback =
      base::OnceCallback<void(const dlp::AddFileResponse response)>;
  using GetFilesSourcesCallback =
      base::OnceCallback<void(const dlp::GetFilesSourcesResponse response)>;
  using CheckFilesTransferCallback =
      base::OnceCallback<void(const dlp::CheckFilesTransferResponse response)>;
  using RequestFileAccessCallback =
      base::OnceCallback<void(const dlp::RequestFileAccessResponse response,
                              base::ScopedFD fd)>;
  using AddFileCall =
      base::RepeatingCallback<void(const dlp::AddFileRequest, AddFileCallback)>;
  using GetFilesSourceCall =
      base::RepeatingCallback<void(const dlp::GetFilesSourcesRequest,
                                   GetFilesSourcesCallback)>;
  using RequestFileAccessCall =
      base::RepeatingCallback<void(const dlp::RequestFileAccessRequest,
                                   RequestFileAccessCallback)>;

  // Interface with testing functionality. Accessed through
  // GetTestInterface(), only implemented in the fake implementation.
  class TestInterface {
   public:
    // Returns how many times |SetDlpFilesPolicyCount| was called.
    virtual int GetSetDlpFilesPolicyCount() const = 0;

    // Sets source url string to be returned for any file inode.
    virtual void SetFakeSource(const std::string&) = 0;

    // Sets CheckFilesTransfer response proto.
    virtual void SetCheckFilesTransferResponse(
        dlp::CheckFilesTransferResponse response) = 0;

    // Sets response for RequestFileAccess call.
    virtual void SetFileAccessAllowed(bool allowed) = 0;

    // Sets the response for IsAlive call.
    virtual void SetIsAlive(bool is_alive) = 0;

    // Sets `mock` used in AddFile calls.
    virtual void SetAddFileMock(AddFileCall mock) = 0;

    // Sets `mock` used in GetFilesSource calls.
    virtual void SetGetFilesSourceMock(GetFilesSourceCall mock) = 0;

    // Returns the last CheckFilesTransferRequest. If it wasn't called, it'll
    // return an empty proto.
    virtual dlp::CheckFilesTransferRequest GetLastCheckFilesTransferRequest()
        const = 0;

    // Sets `mock` used in RequestFileAccess calls.
    virtual void SetRequestFileAccessMock(RequestFileAccessCall mock) = 0;

   protected:
    virtual ~TestInterface() = default;
  };

  DlpClient(const DlpClient&) = delete;
  DlpClient& operator=(const DlpClient&) = delete;

  // Creates and initializes the global instance. |bus| must not be null.
  static void Initialize(dbus::Bus* bus);

  // Creates and initializes a fake global instance if not already created.
  static void InitializeFake();

  // Destroys the global instance.
  static void Shutdown();

  // Returns the global instance which may be null if not initialized.
  static DlpClient* Get();

  // Dlp daemon D-Bus method calls. See org.chromium.Dlp.xml and
  // dlp_service.proto in Chromium OS code for the documentation of the
  // methods and request/response messages.
  virtual void SetDlpFilesPolicy(const dlp::SetDlpFilesPolicyRequest request,
                                 SetDlpFilesPolicyCallback callback) = 0;
  virtual void AddFile(const dlp::AddFileRequest request,
                       AddFileCallback callback) = 0;
  virtual void GetFilesSources(const dlp::GetFilesSourcesRequest request,
                               GetFilesSourcesCallback callback) = 0;
  virtual void CheckFilesTransfer(const dlp::CheckFilesTransferRequest request,
                                  CheckFilesTransferCallback callback) = 0;
  virtual void RequestFileAccess(const dlp::RequestFileAccessRequest request,
                                 RequestFileAccessCallback callback) = 0;

  virtual bool IsAlive() const = 0;

  // Returns an interface for testing (fake only), or returns nullptr.
  virtual TestInterface* GetTestInterface() = 0;

 protected:
  // Initialize/Shutdown should be used instead.
  DlpClient();
  virtual ~DlpClient();
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_DLP_DLP_CLIENT_H_
