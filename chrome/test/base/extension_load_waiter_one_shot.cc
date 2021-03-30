// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/extension_load_waiter_one_shot.h"

#include "base/callback.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/common/extensions/extension_constants.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_service.h"
#include "extensions/browser/extension_host.h"

ExtensionLoadWaiterOneShot::ExtensionLoadWaiterOneShot() = default;

ExtensionLoadWaiterOneShot::~ExtensionLoadWaiterOneShot() = default;

void ExtensionLoadWaiterOneShot::WaitForExtension(const char* extension_id,
                                                  base::OnceClosure load_cb) {
  CHECK(!extension_id_) <<
      "ExtensionLoadWaiterOneShot should only be used once.";
  extension_id_ = extension_id;
  load_looper_ = new content::MessageLoopRunner();
  registrar_.Add(this,
                 extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
                 content::NotificationService::AllSources());
  std::move(load_cb).Run();
  load_looper_->Run();
}

void ExtensionLoadWaiterOneShot::Observe(int type,
                                  const content::NotificationSource& source,
                                  const content::NotificationDetails& details) {
  DCHECK_EQ(extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD, type);

  extensions::ExtensionHost* host =
      content::Details<extensions::ExtensionHost>(details).ptr();
  if (host->extension_id() == extension_id_) {
    browser_context_ = host->browser_context();
    CHECK(browser_context_);
    registrar_.Remove(
        this, extensions::NOTIFICATION_EXTENSION_HOST_DID_STOP_FIRST_LOAD,
        content::NotificationService::AllSources());
    load_looper_->Quit();
  }
}
