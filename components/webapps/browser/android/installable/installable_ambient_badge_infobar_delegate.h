// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE_H_
#define COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE_H_

#include <string>

#include "base/memory/weak_ptr.h"
#include "components/infobars/core/infobar_delegate.h"
#include "components/webapps/browser/android/installable/installable_ambient_badge_client.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "url/gurl.h"

namespace content {
class WebContents;
}
namespace infobars {
class ContentInfoBarManager;
}

namespace webapps {

// Delegate for a infobar shown to users when they visit a progressive web app.
// Tapping the infobar triggers the add to home screen flow.
class InstallableAmbientBadgeInfoBarDelegate
    : public infobars::InfoBarDelegate {
 public:

  InstallableAmbientBadgeInfoBarDelegate(
      const InstallableAmbientBadgeInfoBarDelegate&) = delete;
  InstallableAmbientBadgeInfoBarDelegate& operator=(
      const InstallableAmbientBadgeInfoBarDelegate&) = delete;

  ~InstallableAmbientBadgeInfoBarDelegate() override;

  // Returns a pointer to the InstallableAmbientBadgeInfoBar if it is currently
  // showing. Otherwise returns nullptr.
  static infobars::InfoBar* GetVisibleAmbientBadgeInfoBar(
      infobars::ContentInfoBarManager* infobar_manager);

  // Create and show the infobar.
  static void Create(content::WebContents* web_contents,
                     base::WeakPtr<InstallableAmbientBadgeClient> weak_client,
                     const std::u16string& app_name,
                     const SkBitmap& primary_icon,
                     const bool is_primary_icon_maskable,
                     const GURL& start_url);

  void AddToHomescreen();
  const std::u16string GetMessageText() const;
  const SkBitmap& GetPrimaryIcon() const;
  bool GetIsPrimaryIconMaskable() const;
  const GURL& GetUrl() const { return start_url_; }

 private:
  InstallableAmbientBadgeInfoBarDelegate(
      base::WeakPtr<InstallableAmbientBadgeClient> weak_client,
      const std::u16string& app_name,
      const SkBitmap& primary_icon,
      const bool is_primary_icon_maskable,
      const GURL& start_url);

  // InfoBarDelegate overrides:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  void InfoBarDismissed() override;

  base::WeakPtr<InstallableAmbientBadgeClient> weak_client_;
  const std::u16string app_name_;
  const SkBitmap primary_icon_;
  const bool is_primary_icon_maskable_;
  const GURL start_url_;
};

}  // namespace webapps

#endif  // COMPONENTS_WEBAPPS_BROWSER_ANDROID_INSTALLABLE_INSTALLABLE_AMBIENT_BADGE_INFOBAR_DELEGATE_H_
