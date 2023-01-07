// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_CRYPTO_MODULE_PASSWORD_DIALOG_NSS_H_
#define CHROME_BROWSER_UI_CRYPTO_MODULE_PASSWORD_DIALOG_NSS_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/crypto_module_password_dialog.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/host_port_pair.h"
#include "ui/gfx/native_widget_types.h"

typedef struct CERTCertificateStr CERTCertificate;

namespace chrome {

// Asynchronously unlock |modules|, if necessary. |callback| is called when
// done (regardless if any modules were successfully unlocked or not).  Should
// only be called on UI thread.
void UnlockSlotsIfNecessary(std::vector<crypto::ScopedPK11Slot> modules,
                            CryptoModulePasswordReason reason,
                            const net::HostPortPair& server,
                            gfx::NativeWindow parent,
                            base::OnceClosure callback);

// Asynchronously unlock the |cert|'s module, if necessary. |callback| is
// called when done (regardless if module was successfully unlocked or not).
// Should only be called on UI thread.
void UnlockCertSlotIfNecessary(CERTCertificate* cert,
                               CryptoModulePasswordReason reason,
                               const net::HostPortPair& server,
                               gfx::NativeWindow parent,
                               base::OnceClosure callback);

}  // namespace chrome

#endif  // CHROME_BROWSER_UI_CRYPTO_MODULE_PASSWORD_DIALOG_NSS_H_
