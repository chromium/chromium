// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_MANAGER_H_
#define CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_MANAGER_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/browser_window/public/browser_collection_observer.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/browser/ui/webui/top_chrome/webui_contents_wrapper.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom-forward.h"
#include "third_party/skia/include/core/SkRegion.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/native_ui_types.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class Profile;

namespace omnibox_everywhere {

#if defined(USE_AURA)
class OmniboxEverywhereEventHandlerAura;
#endif
class OmniboxEverywhereWidgetDelegate;

// Manages the desktop Omnibox Everywhere native window (views::Widget)
// lifecycle and handles switching between different profiles.
class OmniboxEverywhereUIManager : public views::WidgetObserver,
                                   public WebUIContentsWrapper::Host,
                                   public BrowserCollectionObserver {
 public:
  DECLARE_CLASS_ELEMENT_IDENTIFIER_VALUE(kOmniboxEverywhereElementId);

  using ContentsWrapperFactory =
      base::RepeatingCallback<std::unique_ptr<WebUIContentsWrapper>(Profile*)>;

  explicit OmniboxEverywhereUIManager(
      ContentsWrapperFactory contents_wrapper_factory = {});
  OmniboxEverywhereUIManager(const OmniboxEverywhereUIManager&) = delete;
  OmniboxEverywhereUIManager& operator=(const OmniboxEverywhereUIManager&) =
      delete;
  ~OmniboxEverywhereUIManager() override;

  void ShowForProfile(Profile* profile,
                      gfx::NativeWindow context = gfx::NativeWindow());

  // Closes the Omnibox Everywhere widget.
  void Close();

  // Synchronously closes the widget and destroys the WebContents during profile
  // shutdown.
  void Shutdown();

  // Returns true if the widget is visible.
  bool IsVisible() const;

  // views::WidgetObserver:
  void OnWidgetActivationChanged(views::Widget* widget, bool active) override;
  void OnWidgetDestroying(views::Widget* widget) override;

  // WebUIContentsWrapper::Host:
  void CloseUI() override;
  void ShowUI() override;
  void ResizeDueToAutoResize(content::WebContents* source,
                             const gfx::Size& new_size) override;
  void RunFileChooser(content::RenderFrameHost* render_frame_host,
                      scoped_refptr<content::FileSelectListener> listener,
                      const blink::mojom::FileChooserParams& params) override;
  void DraggableRegionsChanged(
      const std::vector<blink::mojom::DraggableRegionPtr>& regions,
      content::WebContents* contents) override;

  void OnFileChooserOpened();
  void OnFileChooserClosed();

  void OnDrivePickerOpened();
  void OnDrivePickerClosed();

  // BrowserCollectionObserver:
  void OnBrowserCreated(BrowserWindowInterface* browser) override {}
  void OnBrowserClosed(BrowserWindowInterface* browser) override;
  void OnBrowserActivated(BrowserWindowInterface* browser) override;
  void OnBrowserDeactivated(BrowserWindowInterface* browser) override {}

  void SetIsNavigating(bool is_navigating) { is_navigating_ = is_navigating; }
  bool IsNavigating() const { return is_navigating_; }

  Profile* profile() { return profile_; }
  const Profile* profile() const { return profile_; }
  views::Widget* widget() { return widget_.get(); }
  const views::Widget* widget() const { return widget_.get(); }
  WebUIContentsWrapper* contents_wrapper_for_testing() {
    return contents_wrapper_.get();
  }
  const std::optional<SkRegion>& draggable_region_for_testing() const {
    return draggable_region_;
  }
  bool is_file_chooser_open_for_testing() const {
    return is_file_chooser_open_;
  }
  bool is_drive_picker_open_for_testing() const {
    return is_drive_picker_open_;
  }
  OmniboxEverywhereWidgetDelegate* widget_delegate();
  const OmniboxEverywhereWidgetDelegate* widget_delegate() const;

  bool IsPointInDraggableRegion(const gfx::Point& point) const;

 private:
  content::WebContents* web_contents() const;
  void EnsureContentsWrapperInitialized(Profile* profile);
  void CreateAndInitWidget(gfx::NativeWindow context);
  void ActivateAndFocus();

  std::unique_ptr<WebUIContentsWrapper> CreateContentsWrapper(Profile* profile);

  void CleanUpWidget();
  void OnWidgetClosed(views::Widget::ClosedReason reason);

#if defined(USE_AURA)
  std::unique_ptr<OmniboxEverywhereEventHandlerAura> event_handler_;
#endif

  // The native window hosting the Omnibox Everywhere UI.
  raw_ptr<Profile> profile_ = nullptr;
  ContentsWrapperFactory contents_wrapper_factory_;

  std::unique_ptr<WebUIContentsWrapper> contents_wrapper_;
  std::unique_ptr<OmniboxEverywhereWidgetDelegate> widget_delegate_;
  std::unique_ptr<views::Widget> widget_;

  bool is_file_chooser_open_ = false;
  bool is_drive_picker_open_ = false;
  bool is_navigating_ = false;
  std::optional<SkRegion> draggable_region_;

  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
  base::ScopedObservation<ProfileBrowserCollection, BrowserCollectionObserver>
      browser_collection_observation_{this};
  base::WeakPtrFactory<OmniboxEverywhereUIManager> weak_factory_{this};
};

}  // namespace omnibox_everywhere

#endif  // CHROME_BROWSER_UI_OMNIBOX_OMNIBOX_EVERYWHERE_OMNIBOX_EVERYWHERE_UI_MANAGER_H_
