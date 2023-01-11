// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BLOCKED_CONTENT_ANDROID_POPUP_BLOCKED_INFOBAR_DELEGATE_H_
#define COMPONENTS_BLOCKED_CONTENT_ANDROID_POPUP_BLOCKED_INFOBAR_DELEGATE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/infobars/core/confirm_infobar_delegate.h"
#include "url/gurl.h"

namespace infobars {
class ContentInfoBarManager;
}

class HostContentSettingsMap;

namespace blocked_content {

class PopupBlockedInfoBarDelegate : public ConfirmInfoBarDelegate {
 public:
  // Creates a popup blocked infobar and delegate and adds the infobar to
  // |infobar_manager|. Returns true if the infobar was created, and false if it
  // replaced an existing popup infobar. |on_accept_callback| will be run if the
  // accept button is pressed on the infobar.
  static bool Create(infobars::ContentInfoBarManager* infobar_manager,
                     int num_popups,
                     HostContentSettingsMap* settings_map,
                     base::OnceClosure on_accept_callback);

  ~PopupBlockedInfoBarDelegate() override;

  PopupBlockedInfoBarDelegate(const PopupBlockedInfoBarDelegate&) = delete;
  PopupBlockedInfoBarDelegate& operator=(const PopupBlockedInfoBarDelegate&) =
      delete;

 private:
  PopupBlockedInfoBarDelegate(int num_popups,
                              const GURL& url,
                              HostContentSettingsMap* map,
                              base::OnceClosure on_accept_callback);

  // ConfirmInfoBarDelegate:
  infobars::InfoBarDelegate::InfoBarIdentifier GetIdentifier() const override;
  int GetIconId() const override;
  PopupBlockedInfoBarDelegate* AsPopupBlockedInfoBarDelegate() override;
  std::u16string GetMessageText() const override;
  int GetButtons() const override;
  std::u16string GetButtonLabel(InfoBarButton button) const override;
  bool Accept() override;

  const int num_popups_;
  const GURL url_;
  raw_ptr<HostContentSettingsMap> map_;
  bool can_show_popups_;
  base::OnceClosure on_accept_callback_;
};

}  // namespace blocked_content

#endif  // COMPONENTS_BLOCKED_CONTENT_ANDROID_POPUP_BLOCKED_INFOBAR_DELEGATE_H_
