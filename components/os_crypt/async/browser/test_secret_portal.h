// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OS_CRYPT_ASYNC_BROWSER_TEST_SECRET_PORTAL_H_
#define COMPONENTS_OS_CRYPT_ASYNC_BROWSER_TEST_SECRET_PORTAL_H_

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "dbus/bus.h"
#include "dbus/exported_object.h"
#include "dbus/message.h"
#include "dbus/object_path.h"

namespace os_crypt_async {

// This class implements the service side of the org.freedesktop.portal.Secret
// interface for testing purposes.
class TestSecretPortal {
 public:
  // `pre_test` indicates that this is like a browser first-run.  If true, a new
  // token is given.  If false, a handle to an existing token is expected.
  explicit TestSecretPortal(bool pre_test);

  ~TestSecretPortal();

  std::string BusName() const;

 private:
  scoped_refptr<dbus::Bus> bus_;
  raw_ptr<dbus::ExportedObject> exported_object_ = nullptr;

  void RetrieveSecret(dbus::MethodCall* method_call,
                      dbus::ExportedObject::ResponseSender response_sender);

  const bool pre_test_;

  base::WeakPtrFactory<TestSecretPortal> weak_ptr_factory_{this};
};

}  // namespace os_crypt_async

#endif  // COMPONENTS_OS_CRYPT_ASYNC_BROWSER_TEST_SECRET_PORTAL_H_
