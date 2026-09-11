// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/actions/chrome_action_id.h"
#include "chrome/browser/ui/animation/browser_animation_controller.h"
#include "chrome/browser/ui/animation/browser_animation_types.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/organizer/organizer_panel_state_controller.h"
#include "chrome/browser/ui/views/animations/organizer_panel_animations.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/custom_corners_background.h"
#include "chrome/browser/ui/views/tabs/organizer/layout_constants.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_controls_view.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_utils.h"
#include "chrome/browser/ui/views/tabs/organizer/organizer_panel_view.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/saved_tab_groups/public/features.h"
#include "extensions/buildflags/buildflags.h"
#include "ui/actions/actions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/decoration/shadow.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/actions/action_view_controller.h"
#include "ui/views/background.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_shadow.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"

#if BUILDFLAG(ENABLE_EXTENSIONS)
#include "chrome/browser/extensions/api/side_panel/side_panel_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/ui/views/extensions/extension_view_views.h"
#include "extensions/browser/extension_host.h"
#include "extensions/browser/extension_host_observer.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/common/extension.h"
#endif

#if BUILDFLAG(ENABLE_EXTENSIONS)
class OrganizerPanelView::ExtensionObserverHelper
    : public ExtensionViewViews::Observer,
      public extensions::ExtensionHostObserver {
 public:
  explicit ExtensionObserverHelper(OrganizerPanelView* panel_view)
      : panel_view_(panel_view) {}
  ~ExtensionObserverHelper() override = default;

  void ObserveView(ExtensionViewViews* view) {
    scoped_view_observation_.Reset();
    if (view) {
      scoped_view_observation_.Observe(view);
    }
  }

  void ObserveHost(extensions::ExtensionHost* host) {
    scoped_host_observation_.Reset();
    if (host) {
      scoped_host_observation_.Observe(host);
    }
  }

  void Reset() {
    scoped_view_observation_.Reset();
    scoped_host_observation_.Reset();
  }

  void ResetHost() { scoped_host_observation_.Reset(); }

  // ExtensionViewViews::Observer:
  void OnViewDestroying() override { panel_view_->OnViewDestroying(); }

  // extensions::ExtensionHostObserver:
  void OnExtensionHostDestroyed(extensions::ExtensionHost* host) override {
    panel_view_->OnExtensionHostDestroyed(host);
  }

 private:
  const raw_ptr<OrganizerPanelView> panel_view_;
  base::ScopedObservation<ExtensionViewViews, ExtensionViewViews::Observer>
      scoped_view_observation_{this};
  base::ScopedObservation<extensions::ExtensionHost,
                          extensions::ExtensionHostObserver>
      scoped_host_observation_{this};
};
#endif

OrganizerPanelView::OrganizerPanelView(
    BrowserWindowInterface* browser,
    actions::ActionItem* root_action_item,
    OrganizerPanelStateController* state_controller)
    : browser_(browser),
      root_action_item_(root_action_item),
      state_controller_subscription_(state_controller->RegisterOnStateChanged(
          base::BindRepeating(&OrganizerPanelView::OnOrganizerPanelStateChanged,
                              base::Unretained(this)))),
      animation_subscription_(
          BrowserAnimationController::From(browser)->Subscribe(
              OrganizerPanelAnimations::kOrganizerPanel,
              base::BindRepeating(
                  [](OrganizerPanelView* view,
                     const BrowserAnimationController*,
                     BrowserAnimationUpdate) { view->InvalidateLayout(); },
                  base::Unretained(this)))) {
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetIsFastRoundedCorner(true);

  SetPreferredSize(gfx::Size(organizer_panel::kOrganizerPanelMinWidth, 0));
  SetProperty(views::kElementIdentifierKey, kOrganizerPanelViewElementId);

  auto& accessibility = GetViewAccessibility();
  accessibility.SetRole(ax::mojom::Role::kPane);
  accessibility.SetName(l10n_util::GetStringUTF16(IDS_ORGANIZER_PANEL));
  SetFocusBehavior(FocusBehavior::NEVER);

  const bool show_extensions =
      organizer_panel::IsShowExtensionsSidePanelUiInOrganizerPanelEnabled();

  if (browser_ && browser_->GetProfile() && !show_extensions) {
    auto web_view = std::make_unique<views::WebView>(browser_->GetProfile());
    webui::SetBrowserWindowInterface(web_view->GetWebContents(), browser_);
    views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
        web_view->GetWebContents(), SK_ColorTRANSPARENT);
    web_view->LoadInitialURL(GURL(chrome::kChromeUIOrganizerPanelURL));
    web_view->SetProperty(
        views::kFlexBehaviorKey,
        views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                                 views::MaximumFlexSizeRule::kUnbounded));
    web_view_ = AddChildView(std::move(web_view));
  }

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (show_extensions) {
    extension_observer_helper_ =
        std::make_unique<ExtensionObserverHelper>(this);
  }
#endif
}

OrganizerPanelView::~OrganizerPanelView() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  ResetExtensionContent();
#endif
}

void OrganizerPanelView::OnOrganizerPanelStateChanged(
    OrganizerPanelStateController* state_controller) {

  const bool visible = state_controller->IsOrganizerPanelVisible();

#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (visible &&
      organizer_panel::IsShowExtensionsSidePanelUiInOrganizerPanelEnabled()) {
    if (state_controller->active_extension_id().has_value()) {
      UpdateExtensionContent(*state_controller->active_extension_id());
    } else {
      UpdateDefaultExtensionContent();
    }
  }
#endif

  if (visible) {
    last_opened_time_ = base::TimeTicks::Now();
  } else {
    base::TimeDelta open_duration = base::TimeTicks::Now() - last_opened_time_;
    base::UmaHistogramCustomCounts("Projects.ProjectsPanel.TimeOpen",
                                   open_duration.InSeconds(), 1,
                                   base::Minutes(5).InSeconds(), 50);
  }
}

// static
views::View* OrganizerPanelView::GetWebViewForTesting() {
#if BUILDFLAG(ENABLE_EXTENSIONS)
  if (extension_view_) {
    return extension_view_;
  }
#endif
  return web_view_;
}

void OrganizerPanelView::Layout(PassKey) {
  LayoutSuperclass<views::View>(this);

  // Set clip region based on parent.
  if (parent()) {
    const gfx::Rect clip_bounds = GetLocalBounds();
    const gfx::Rect parent_bounds = views::View::ConvertRectToTarget(
        parent(), this, parent()->GetLocalBounds());
    gfx::Rect clip_rect = clip_bounds;
    clip_rect.Intersect(parent_bounds);
    layer()->SetClipRect(clip_rect);
    gfx::RoundedCornersF corners;
    if (parent()->background()) {
      if (auto* const background =
              parent()->background()->AsA<CustomCornersBackground>();
          background && background->is_visible()) {
        // Note: this mirrors for RtL.
        corners = background->GetRoundedCornerRadii().value_or(
            gfx::RoundedCornersF());
        // These bounds are also mirrored for RtL.
        if (clip_bounds.x() < parent_bounds.x()) {
          corners.set_upper_left(0.f);
          corners.set_lower_left(0.f);
        }
        if (clip_bounds.right() > parent_bounds.right()) {
          corners.set_upper_right(0.f);
          corners.set_lower_right(0.f);
        }
      }
    }
    layer()->SetRoundedCornerRadius(corners);
  }
}

void OrganizerPanelView::ClosePanel() {
  actions::ActionItem* action_item = actions::ActionManager::Get().FindAction(
      kActionToggleOrganizerPanel, root_action_item_);
  if (action_item) {
    action_item->InvokeAction();
  }
}

#if BUILDFLAG(ENABLE_EXTENSIONS)
void OrganizerPanelView::HandleCloseExtensionHost(
    extensions::ExtensionHost* host) {
  ClosePanel();
}

void OrganizerPanelView::UpdateExtensionContent(
    const extensions::ExtensionId& extension_id) {
  if (current_extension_id_ == extension_id && extension_view_ &&
      extension_host_) {
    return;
  }

  ResetExtensionContent();

  if (!browser_ || !browser_->GetProfile()) {
    return;
  }

  Profile* profile = browser_->GetProfile();
  const extensions::Extension* extension =
      extensions::ExtensionRegistry::Get(profile)->enabled_extensions().GetByID(
          extension_id);
  if (!extension) {
    return;
  }

  extensions::SidePanelService* service =
      extensions::SidePanelService::Get(profile);
  if (!service) {
    return;
  }

  auto options = service->GetOptions(*extension, /*tab_id=*/std::nullopt);
  if (!options.enabled.value_or(false) || !options.path.has_value()) {
    return;
  }

  GURL side_panel_url = GURL(*options.path);
  if (!side_panel_url.SchemeIsHTTPOrHTTPS()) {
    side_panel_url = extension->ResolveExtensionURL(*options.path);
  }

  extension_host_ = extensions::ExtensionViewHostFactory::CreateSidePanelHost(
      *extension, side_panel_url, browser_, /*tab_interface=*/nullptr);
  if (!extension_host_) {
    return;
  }

  // Handle window.close() inside extension side panel.
  extension_host_->SetCloseHandler(base::BindOnce(
      &OrganizerPanelView::HandleCloseExtensionHost, base::Unretained(this)));

  auto extension_view =
      std::make_unique<ExtensionViewViews>(profile, extension_host_.get());
  extension_view->Init();
  extension_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));

  extension_view_ = AddChildView(std::move(extension_view));
  current_extension_id_ = extension_id;

  if (extension_observer_helper_) {
    extension_observer_helper_->ObserveView(extension_view_.get());
    extension_observer_helper_->ObserveHost(extension_host_.get());
  }

  service->DispatchOnOpenedEvent(
      extension_id, extensions::ExtensionTabUtil::GetWindowId(browser_),
      /*tab_id=*/std::nullopt, side_panel_url.GetPath());
}

void OrganizerPanelView::UpdateDefaultExtensionContent() {
  if (!browser_ || !browser_->GetProfile()) {
    return;
  }

  auto* registry = extensions::ExtensionRegistry::Get(browser_->GetProfile());
  auto* service = extensions::SidePanelService::Get(browser_->GetProfile());
  if (!registry || !service) {
    return;
  }

  for (const auto& extension : registry->enabled_extensions()) {
    auto options = service->GetOptions(*extension, std::nullopt);
    if (options.enabled.value_or(false) && options.path.has_value()) {
      UpdateExtensionContent(extension->id());
      return;
    }
  }
}

void OrganizerPanelView::ResetExtensionContent() {
  if (current_extension_id_ && extension_host_ && browser_ &&
      browser_->GetProfile() && !browser_->IsDeleteScheduled()) {
    if (auto* const service =
            extensions::SidePanelService::Get(browser_->GetProfile())) {
      service->DispatchOnClosedEvent(
          *current_extension_id_,
          extensions::ExtensionTabUtil::GetWindowId(browser_),
          /*tab_id=*/std::nullopt, extension_host_->initial_url().GetPath());
    }
  }

  if (extension_observer_helper_) {
    extension_observer_helper_->Reset();
  }
  if (extension_view_) {
    auto to_release = RemoveChildViewT(extension_view_);
    extension_view_ = nullptr;
  }
  extension_host_.reset();
  current_extension_id_.reset();
}

void OrganizerPanelView::OnViewDestroying() {
  // Clear `extension_view_` so ResetExtensionContent() will not attempt to
  // remove or delete a view that is already being destroyed.
  extension_view_ = nullptr;
  ResetExtensionContent();
}

void OrganizerPanelView::OnExtensionHostDestroyed(
    extensions::ExtensionHost* host) {
  DCHECK_EQ(extension_host_.get(), host);
  if (extension_observer_helper_) {
    extension_observer_helper_->ResetHost();
  }
  if (current_extension_id_ && browser_ && browser_->GetProfile()) {
    auto* service = extensions::SidePanelService::Get(browser_->GetProfile());
    if (service) {
      service->DispatchOnClosedEvent(
          *current_extension_id_,
          extensions::ExtensionTabUtil::GetWindowId(browser_),
          /*tab_id=*/std::nullopt, host->initial_url().GetPath());
    }
  }
  // Release ownership because `host` is currently executing its destructor.
  extension_host_.release();
  ResetExtensionContent();
}
#endif

BEGIN_METADATA(OrganizerPanelView)
END_METADATA
