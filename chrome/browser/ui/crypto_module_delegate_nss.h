// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CRYPTO_MODULE_DELEGATE_NSS_H_
#define CHROME_BROWSER_UI_CRYPTO_MODULE_DELEGATE_NSS_H_

#include <string>

#include "base/synchronization/waitable_event.h"
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#include "crypto/nss_crypto_module_delegate.h"
#include "net/base/host_port_pair.h"

// Delegate to handle unlocking a slot or indicating which slot to store a key
// in. When passing to NSS functions which take a wincx argument, use the value
// returned from the wincx() method.
class ChromeNSSCryptoModuleDelegate
    : public crypto::CryptoModuleBlockingPasswordDelegate {
 public:
  // Create a ChromeNSSCryptoModuleDelegate. |reason| is used to select what
  // string to show the user, |server| is displayed to indicate which connection
  // is causing the dialog to appear. |slot| can be NULL.
  ChromeNSSCryptoModuleDelegate(CryptoModulePasswordReason reason,
                                const net::HostPortPair& server);

  ChromeNSSCryptoModuleDelegate(const ChromeNSSCryptoModuleDelegate&) = delete;
  ChromeNSSCryptoModuleDelegate& operator=(
      const ChromeNSSCryptoModuleDelegate&) = delete;

  // crypto::CryptoModuleBlockingPasswordDelegate implementation.
  std::string RequestPassword(const std::string& slot_name,
                              bool retry,
                              bool* cancelled) override;

 private:
  friend class base::RefCountedThreadSafe<ChromeNSSCryptoModuleDelegate>;

  ~ChromeNSSCryptoModuleDelegate() override;

  void ShowDialog(const std::string& slot_name, bool retry);

  void GotPassword(const std::string& password);

  // Parameters displayed in the dialog.
  const CryptoModulePasswordReason reason_;
  net::HostPortPair server_;

  // Event to block worker thread while waiting for dialog on UI thread.
  base::WaitableEvent event_;

  // Stores the results from the dialog for access on worker thread.
  std::string password_;
  bool cancelled_;
};

// Create a delegate which only handles unlocking slots.
crypto::CryptoModuleBlockingPasswordDelegate*
CreateCryptoModuleBlockingPasswordDelegate(CryptoModulePasswordReason reason,
                                           const net::HostPortPair& server);

#endif  // CHROME_BROWSER_UI_CRYPTO_MODULE_DELEGATE_NSS_H_
