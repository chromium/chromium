// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/crypto_module_delegate_nss.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread_restrictions.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

using content::BrowserThread;

ChromeNSSCryptoModuleDelegate::ChromeNSSCryptoModuleDelegate(
    CryptoModulePasswordReason reason,
    const net::HostPortPair& server)
    : reason_(reason),
      server_(server),
      event_(base::WaitableEvent::ResetPolicy::AUTOMATIC,
             base::WaitableEvent::InitialState::NOT_SIGNALED),
      cancelled_(false) {}

ChromeNSSCryptoModuleDelegate::~ChromeNSSCryptoModuleDelegate() {}

std::string ChromeNSSCryptoModuleDelegate::RequestPassword(
    const std::string& slot_name,
    bool retry,
    bool* cancelled) {
  DCHECK(!event_.IsSignaled());
  event_.Reset();

  if (content::GetUIThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&ChromeNSSCryptoModuleDelegate::ShowDialog,
                         // This method blocks on |event_| until the task
                         // completes, so there's no need to ref-count.
                         base::Unretained(this), slot_name, retry))) {
    // This should always be invoked on a worker sequence with the
    // base::MayBlock() trait.
    base::ScopedAllowBaseSyncPrimitives allow_wait;
    event_.Wait();
  }
  *cancelled = cancelled_;
  return password_;
}

void ChromeNSSCryptoModuleDelegate::ShowDialog(const std::string& slot_name,
                                               bool retry) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ShowCryptoModulePasswordDialog(
      slot_name, retry, reason_, server_.host(),
      nullptr,  // TODO(mattm): Supply parent window.
      base::BindOnce(&ChromeNSSCryptoModuleDelegate::GotPassword,
                     // RequestPassword is blocked on |event_| until GotPassword
                     // is called, so there's no need to ref-count.
                     base::Unretained(this)));
}

void ChromeNSSCryptoModuleDelegate::GotPassword(const std::string& password) {
  if (!password.empty())
    password_ = password;
  else
    cancelled_ = true;
  event_.Signal();
}

crypto::CryptoModuleBlockingPasswordDelegate*
CreateCryptoModuleBlockingPasswordDelegate(CryptoModulePasswordReason reason,
                                           const net::HostPortPair& server) {
  return new ChromeNSSCryptoModuleDelegate(reason, server);
}
