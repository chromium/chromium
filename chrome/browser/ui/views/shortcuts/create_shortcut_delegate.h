// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_SHORTCUTS_CREATE_SHORTCUT_DELEGATE_H_
#define CHROME_BROWSER_UI_VIEWS_SHORTCUTS_CREATE_SHORTCUT_DELEGATE_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/views/controls/site_icon_text_and_origin_view.h"
#include "content/public/browser/visibility.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/models/dialog_model.h"

namespace content {
class WebContents;
class NavigationHandle;
}  // namespace content

namespace shortcuts {

class CreateShortcutDelegate : public ui::DialogModelDelegate,
                               public content::WebContentsObserver {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kCreateShortcutDialogOkButtonId);

  CreateShortcutDelegate(content::WebContents* web_contents,
                         chrome::CreateShortcutDialogCallback final_callback);

  ~CreateShortcutDelegate() override;

  void OnAccept();
  void OnClose();
  void OnTitleUpdated(const std::u16string& trimmed_text_field_data);

  base::WeakPtr<CreateShortcutDelegate> AsWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // content::WebContentsObserver:
  void OnVisibilityChanged(content::Visibility visibility) override;
  void WebContentsDestroyed() override;
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override;

 private:
  void CloseDialogAsIgnored();

  chrome::CreateShortcutDialogCallback final_callback_;
  std::u16string text_field_data_;
  base::WeakPtrFactory<CreateShortcutDelegate> weak_ptr_factory_{this};
};

}  // namespace shortcuts

#endif  // CHROME_BROWSER_UI_VIEWS_SHORTCUTS_CREATE_SHORTCUT_DELEGATE_H_
