// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_NOTIFICATION_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_NOTIFICATION_H_

#include <string>

#include "chrome/browser/profiles/profile.h"
#include "extensions/common/extension.h"
#include "ui/message_center/public/cpp/notification_delegate.h"

class ExtensionInstalledNotification
    : public message_center::NotificationDelegate {
 public:
  static void Show(const extensions::Extension* extension, Profile* profile);

  ExtensionInstalledNotification(const extensions::Extension* extension,
                                 Profile* profile);

  // NotificationDelegate override:
  void Click(const absl::optional<int>& button_index,
             const absl::optional<std::u16string>& reply) override;

 protected:
  // This class is ref-counted.
  ~ExtensionInstalledNotification() override;

 private:
  const std::string extension_id_;
  Profile* const profile_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionInstalledNotification);
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_INSTALLED_NOTIFICATION_H_
