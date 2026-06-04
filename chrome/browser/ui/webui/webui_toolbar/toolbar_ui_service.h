// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_TOOLBAR_UI_SERVICE_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_TOOLBAR_UI_SERVICE_H_

#include <variant>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/types/expected.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/icon_table_fetcher.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api.mojom.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote_set.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "ui/base/mojom/menu_source_type.mojom-shared.h"
#include "ui/base/pointer/touch_ui_controller.h"
#include "ui/gfx/geometry/rect_f.h"

class MetricsReporter;

namespace toolbar_ui_api {

class ToolbarUIService : public toolbar_ui_api::mojom::ToolbarUIService {
 public:
  class ToolbarUIServiceDelegate {
   public:
    virtual ~ToolbarUIServiceDelegate() = default;
    virtual void HandleContextMenu(
        toolbar_ui_api::mojom::ContextMenuType menu_type,
        const gfx::RectF& bounds_in_css_pixels,
        ui::mojom::MenuSourceType source) = 0;
    virtual void ShowContentSettingsBubble(
        ::toolbar_ui_api::mojom::ContentSettingImageType type,
        ShowContentSettingsBubbleCallback callback) = 0;
    virtual void OnPageInitialized() = 0;
    virtual void InvokePinnedToolbarAction(
        toolbar_ui_api::mojom::PinnedToolbarAction action_id) = 0;
    virtual void OnLhsChipMousePressed(
        toolbar_ui_api::mojom::LhsChipIdentifier identifier) = 0;
    virtual void OnLhsChipClicked(
        toolbar_ui_api::mojom::LhsChipIdentifier identifier,
        bool is_mouse_interaction) = 0;
    virtual void OnLhsChipPointerEntered(
        toolbar_ui_api::mojom::LhsChipIdentifier identifier) = 0;
    virtual void OnLhsChipPointerExited(
        toolbar_ui_api::mojom::LhsChipIdentifier identifier) = 0;
    virtual void OnLhsChipExpandAnimationEnded(
        toolbar_ui_api::mojom::LhsChipIdentifier identifier) = 0;
    virtual void OnLhsChipCollapseAnimationEnded(
        toolbar_ui_api::mojom::LhsChipIdentifier identifier) = 0;
    virtual void OnLhsChipDrag(
        toolbar_ui_api::mojom::LhsChipIdentifier identifier,
        ui::mojom::DragEventSource source) = 0;
    virtual void OnHomeButtonDropUrl(const GURL& url) = 0;
    virtual void OnHomeButtonDropFile(const gfx::PointF& drop_position) = 0;
    virtual void OnToolbarDropFile(const gfx::PointF& drop_position) = 0;
    virtual base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
    OnOmniboxAction(toolbar_ui_api::mojom::OmniboxActionPtr action) = 0;
    virtual void ShowAvatarMenu() = 0;
  };

  ToolbarUIService(
      mojo::PendingReceiver<toolbar_ui_api::mojom::ToolbarUIService> service,
      std::unique_ptr<NavigationControlsStateFetcher> state_fetcher,
      std::unique_ptr<IconTableFetcher> icon_table_fetcher,
      MetricsReporter* metrics_reporter,
      ToolbarUIServiceDelegate* delegate);

  ToolbarUIService(const ToolbarUIService&) = delete;
  ToolbarUIService& operator=(const ToolbarUIService&) = delete;

  ~ToolbarUIService() override;

  void SetDelegate(ToolbarUIServiceDelegate* delegate);

  void OnNavigationControlsStateChanged(
      const mojom::NavigationControlsState& state);
  void OnFocusRequested(toolbar_ui_api::mojom::FocusRequestTarget target);

  // toolbar_ui_api::mojom::ToolbarUIService:
  void Bind(BindCallback callback) override;
  void ShowContextMenu(toolbar_ui_api::mojom::ContextMenuType menu_type,
                       const gfx::RectF& bounds_in_css_pixels,
                       ui::mojom::MenuSourceType source) override;
  void OnOmniboxAction(toolbar_ui_api::mojom::OmniboxActionPtr action,
                       OnOmniboxActionCallback callback) override;
  void OnPageInitialized() override;
  void ShowContentSettingsBubble(
      ::toolbar_ui_api::mojom::ContentSettingImageType type,
      ShowContentSettingsBubbleCallback callback) override;
  void InvokePinnedToolbarAction(
      toolbar_ui_api::mojom::PinnedToolbarAction action_id) override;
  void OnLhsChipMousePressed(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;
  void OnLhsChipClicked(toolbar_ui_api::mojom::LhsChipIdentifier identifier,
                        bool is_mouse_interaction) override;
  void OnLhsChipPointerEntered(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;
  void OnLhsChipPointerExited(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;
  void OnLhsChipExpandAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;
  void OnLhsChipCollapseAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) override;
  void OnLhsChipDrag(toolbar_ui_api::mojom::LhsChipIdentifier identifier,
                     ui::mojom::DragEventSource source) override;
  void OnHomeButtonDropUrl(const GURL& url) override;
  void OnHomeButtonDropFile(const gfx::PointF& drop_position) override;
  void OnToolbarDropFile(const gfx::PointF& drop_position) override;
  void ShowAvatarMenu(ShowAvatarMenuCallback callback) override;

 private:
  mojo::Receiver<toolbar_ui_api::mojom::ToolbarUIService> service_;
  mojo::RemoteSet<toolbar_ui_api::mojom::ToolbarUIObserver> observers_;

  std::unique_ptr<NavigationControlsStateFetcher> state_fetcher_;
  std::unique_ptr<IconTableFetcher> icon_table_fetcher_;

  // Not owned.
  raw_ptr<MetricsReporter> metrics_reporter_;
  raw_ptr<ToolbarUIServiceDelegate> delegate_;

  // Must be the last member.
  base::WeakPtrFactory<ToolbarUIService> weak_ptr_factory_{this};
};

}  // namespace toolbar_ui_api

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_TOOLBAR_UI_SERVICE_H_
