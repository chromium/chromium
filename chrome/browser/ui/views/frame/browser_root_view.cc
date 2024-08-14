// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_root_view.h"

#include <cmath>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_op.h"
#include "base/containers/adapters.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/user_metrics.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/autocomplete/autocomplete_classifier_factory.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/tabs/features.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_user_gesture_details.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/tab_strip_region_view.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/common/chrome_features.h"
#include "components/omnibox/browser/autocomplete_classifier.h"
#include "components/omnibox/browser/autocomplete_match.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/webplugininfo.h"
#include "net/base/filename_util.h"
#include "net/base/mime_util.h"
#include "ppapi/buildflags/buildflags.h"
#include "third_party/blink/public/common/mime_util/mime_util.h"
#include "third_party/metrics_proto/omnibox_event.pb.h"
#include "ui/base/clipboard/clipboard_constants.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/drop_target_event.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom.h"
#include "ui/base/dragdrop/os_exchange_data.h"
#include "ui/base/hit_test.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer_tree_owner.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/views/view.h"
#include "url/url_constants.h"

#if BUILDFLAG(ENABLE_PLUGINS)
#include "content/public/browser/plugin_service.h"
#endif

namespace {

using ::ui::mojom::DragOperation;

// An increasing sequence number used to initialize the `sequence` member
// variable of `DropInfo`. Because a background task is posted to process URLs,
// a consistent sequence number is used to ensure that the `DropInfo` that
// initiated the task is the same one that is filled in with the results.
int gDropInfoSequence = 0;

// Get the MIME types of the files pointed to by `urls`, based on the files'
// extensions. Must be called in a context that allows blocking.
std::vector<std::string> GetURLMimeTypes(const std::vector<GURL>& urls) {
  std::vector<std::string> mime_types;

  for (const auto& url : urls) {
    if (!url.SchemeIsFile()) {
      mime_types.emplace_back();
      continue;
    }

    base::FilePath full_path;
    if (!net::FileURLToFilePath(url, &full_path)) {
      mime_types.emplace_back();
      continue;
    }

    std::string mime_type;
    // This call may block on some platforms.
    if (!net::GetMimeTypeFromFile(full_path, &mime_type)) {
      mime_types.emplace_back();
      continue;
    }

    mime_types.push_back(std::move(mime_type));
  }

  return mime_types;
}

// Filters `urls` for whether they should be allowed for drops. `mime_types` is
// the output from a call to `GetURLMimeTypes()` as a background task, and must
// contain a 1:1 list of the MIME types of the corresponding URLs, with an empty
// string for URLs that aren't file URLs or for those whose MIME type could not
// be obtained. `browser_context` is the BrowserContext to use to look up
// support for MIME types in plugins. When the filtering is complete, `callback`
// will be called with the final list of URLs to use for the drop.
void FilterURLsForDropability(
    const std::vector<GURL>& urls,
    content::BrowserContext* browser_context,
    base::OnceCallback<void(std::vector<GURL> urls)> callback,
    const std::vector<std::string>& mime_types) {
  CHECK_EQ(urls.size(), mime_types.size());
  std::vector<GURL> filtered_urls;

  for (size_t i = 0; i < urls.size(); ++i) {
    const GURL& url = urls[i];
    const std::string& mime_type = mime_types[i];

    // Disallow javascript: URLs to prevent self-XSS.
    if (url.SchemeIs(url::kJavaScriptScheme)) {
      continue;
    }

    // Check whether the mime types, if given, are known to be supported or
    // whether there is a plugin that supports the mime type (e.g. PDF).
    // TODO(bauerb): This possibly uses stale information, but it's guaranteed
    // not to do disk access.
    bool supported = mime_type.empty() || blink::IsSupportedMimeType(mime_type);
#if BUILDFLAG(ENABLE_PLUGINS)
    content::WebPluginInfo plugin;
    supported =
        supported ||
        content::PluginService::GetInstance()->GetPluginInfo(
            browser_context, url, mime_type, /*allow_wildcard=*/false,
            /*is_stale=*/nullptr, &plugin, /*actual_mime_type=*/nullptr);
#endif

    if (supported) {
      filtered_urls.push_back(url);
    }
  }

  std::move(callback).Run(filtered_urls);
}

// Returns the URLs that are currently being dragged by the user and which
// should be considered for the drop.
std::vector<GURL> GetURLsForDrop(const ui::DropTargetEvent& event) {
  std::optional<std::vector<GURL>> urls =
      event.data().GetURLs(ui::FilenameToURLPolicy::CONVERT_FILENAMES);
  if (!urls.has_value()) {
    return {};
  }

  std::erase_if(urls.value(), [](const GURL& url) { return !url.is_valid(); });

  return urls.value();
}

// Converts from `ui::DragDropTypes` to `::ui::mojom::DragOperation`.
DragOperation GetDropEffect(const ui::DropTargetEvent& event) {
  const int source_ops = event.source_operations();
  if (source_ops & ui::DragDropTypes::DRAG_COPY) {
    return DragOperation::kCopy;
  }
  if (source_ops & ui::DragDropTypes::DRAG_LINK) {
    return DragOperation::kLink;
  }
  return DragOperation::kMove;
}

}  // namespace

BrowserRootView::DropInfo::DropInfo() = default;

BrowserRootView::DropInfo::~DropInfo() {
  if (target) {
    target->HandleDragExited();
  }
}

BrowserRootView::BrowserRootView(BrowserView* browser_view,
                                 views::Widget* widget)
    : views::internal::RootView(widget), browser_view_(browser_view) {}

BrowserRootView::~BrowserRootView() {
  // It's possible to destroy the browser while a drop is active.  In this case,
  // |drop_info_| will be non-null, but its |target| likely points to an
  // already-deleted child.  Clear the target so ~DropInfo() will not try and
  // notify it of the drag ending. http://crbug.com/1001942
  if (drop_info_) {
    drop_info_->target = nullptr;
  }
}

bool BrowserRootView::GetDropFormats(
    int* formats,
    std::set<ui::ClipboardFormatType>* format_types) {
  if (tabstrip()->GetVisible() || toolbar()->GetVisible()) {
    *formats = ui::OSExchangeData::URL | ui::OSExchangeData::STRING;
    return true;
  }
  return false;
}

bool BrowserRootView::AreDropTypesRequired() {
  return true;
}

bool BrowserRootView::CanDrop(const ui::OSExchangeData& data) {
  // If it's not tabbed browser, we don't have to support drag and drops.
  if (!browser_view_->GetIsNormalType()) {
    return false;
  }

  if (!tabstrip()->GetVisible() && !toolbar()->GetVisible()) {
    return false;
  }

  // If this is for a fallback window dragging session, return false and let
  // TabStripRegionView forward drag events to TabDragController. This is
  // necessary because we don't want to return true if the custom MIME type is
  // there but the mouse is not over the tab strip region, and we don't know the
  // current mouse location.
  // TODO(crbug.com/40828528): This is a smoking gun code smell;
  // TabStripRegionView and Toolbar have different affordances, so they should
  // separately override the drag&drop methods.
  if (data.HasCustomFormat(
          ui::ClipboardFormatType::GetType(ui::kMimeTypeWindowDrag))) {
    return false;
  }

  // If there is a URL, we'll allow the drop.
  if (data.HasURL(ui::FilenameToURLPolicy::CONVERT_FILENAMES)) {
    return true;
  }

  // If there isn't a URL, allow a drop if 'paste and go' can convert to a URL.
  return GetPasteAndGoURL(data).has_value();
}

void BrowserRootView::OnDragEntered(const ui::DropTargetEvent& event) {
  drop_info_ = std::make_unique<DropInfo>();
  drop_info_->sequence = ++gDropInfoSequence;

  std::vector<GURL> urls = GetURLsForDrop(event);

  // If there aren't any proper URLs, allow a 'paste and go' conversion of text
  // content to a URL.
  if (urls.empty()) {
    const std::optional<GURL> paste_and_go_url = GetPasteAndGoURL(event.data());
    if (paste_and_go_url.has_value()) {
      urls.push_back(paste_and_go_url.value());
    }
  }

  // Avoid crashing while the tab strip is being initialized or is empty.
  content::WebContents* web_contents =
      browser_view_->browser()->tab_strip_model()->GetActiveWebContents();
  if (!web_contents) {
    return;
  }

  // Filter all the URLs.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&GetURLMimeTypes, urls),
      base::BindOnce(&FilterURLsForDropability, urls,
                     browser_view_->browser()->profile(),
                     base::BindOnce(&BrowserRootView::OnFilteringComplete,
                                    weak_ptr_factory_.GetWeakPtr(),
                                    drop_info_->sequence)));
}

int BrowserRootView::OnDragUpdated(const ui::DropTargetEvent& event) {
  if (!drop_info_) {
    OnDragEntered(event);
  }

  if (auto* drop_target = GetDropTarget(event)) {
    if (drop_info_->target && drop_info_->target != drop_target) {
      drop_info_->target->HandleDragExited();
    }
    drop_info_->target = drop_target;

    if (drop_info_->filtering_complete && !drop_info_->urls.empty()) {
      drop_info_->index =
          GetDropIndexForEvent(event, event.data(), drop_target);
    } else {
      drop_info_->index.reset();
    }

    drop_target->HandleDragUpdate(drop_info_->index);
    return drop_info_->index ? static_cast<int>(GetDropEffect(event))
                             : ui::DragDropTypes::DRAG_NONE;
  }

  OnDragExited();
  return ui::DragDropTypes::DRAG_NONE;
}

void BrowserRootView::OnDragExited() {
  drop_info_.reset();
}

views::View::DropCallback BrowserRootView::GetDropCallback(
    const ui::DropTargetEvent& event) {
  if (!drop_info_) {
    return base::DoNothing();
  }

  // Moving `drop_info_` ensures we call HandleDragExited() on |drop_info_|'s
  // |target| when this function returns.
  return base::BindOnce(&BrowserRootView::NavigateToDroppedUrls,
                        weak_ptr_factory_.GetWeakPtr(), std::move(drop_info_));
}

bool BrowserRootView::OnMouseWheel(const ui::MouseWheelEvent& event) {
  // TODO(dfried): See if it's possible to move this logic deeper into the view
  // hierarchy - ideally to TabStripRegionView.

  // Scroll-event-changes-tab is incompatible with scrolling tabstrip, so
  // disable it if the latter feature is enabled.
  if (browser_defaults::kScrollEventChangesTab &&
      !base::FeatureList::IsEnabled(tabs::kScrollableTabStrip)) {
    // Switch to the left/right tab if the wheel-scroll happens over the
    // tabstrip, or the empty space beside the tabstrip.
    views::View* hit_view = GetEventHandlerForPoint(event.location());
    int hittest =
        GetWidget()->non_client_view()->NonClientHitTest(event.location());
    if (tabstrip()->Contains(hit_view) || hittest == HTCAPTION ||
        hittest == HTTOP) {
      scroll_remainder_x_ += event.x_offset();
      scroll_remainder_y_ += event.y_offset();

      // Number of integer scroll events that have passed in each direction.
      int whole_scroll_amount_x =
          std::lround(static_cast<double>(scroll_remainder_x_) /
                      ui::MouseWheelEvent::kWheelDelta);
      int whole_scroll_amount_y =
          std::lround(static_cast<double>(scroll_remainder_y_) /
                      ui::MouseWheelEvent::kWheelDelta);

      // Adjust the remainder such that any whole scrolls we have taken action
      // for don't count towards the scroll remainder.
      scroll_remainder_x_ -=
          whole_scroll_amount_x * ui::MouseWheelEvent::kWheelDelta;
      scroll_remainder_y_ -=
          whole_scroll_amount_y * ui::MouseWheelEvent::kWheelDelta;

      // Count a scroll in either axis - summing the axes works for this.
      int whole_scroll_offset = whole_scroll_amount_x + whole_scroll_amount_y;

      Browser* browser = browser_view_->browser();
      TabStripModel* model = browser->tab_strip_model();

      auto has_tab_in_direction = [model](int delta) {
        for (int index = model->active_index() + delta;
             model->ContainsIndex(index); index += delta) {
          if (!model->IsTabCollapsed(index)) {
            return true;
          }
        }
        return false;
      };

      // Switch to the next tab only if not at the end of the tab-strip.
      if (whole_scroll_offset < 0 && has_tab_in_direction(1)) {
        chrome::SelectNextTab(
            browser, TabStripUserGestureDetails(
                         TabStripUserGestureDetails::GestureType::kWheel,
                         event.time_stamp()));
        return true;
      }

      // Switch to the previous tab only if not at the beginning of the
      // tab-strip.
      if (whole_scroll_offset > 0 && has_tab_in_direction(-1)) {
        chrome::SelectPreviousTab(
            browser, TabStripUserGestureDetails(
                         TabStripUserGestureDetails::GestureType::kWheel,
                         event.time_stamp()));
        return true;
      }
    }
  }
  return RootView::OnMouseWheel(event);
}

void BrowserRootView::OnMouseExited(const ui::MouseEvent& event) {
  // Reset the remainders so tab switches occur halfway through a smooth scroll.
  scroll_remainder_x_ = 0;
  scroll_remainder_y_ = 0;
  RootView::OnMouseExited(event);
}

gfx::Size BrowserRootView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return browser_view_->GetRestoredBounds().size();
}

void BrowserRootView::PaintChildren(const views::PaintInfo& paint_info) {
  views::internal::RootView::PaintChildren(paint_info);

  // ToolbarView can't paint its own top stroke because the stroke is drawn just
  // above its bounds, where the active tab can overwrite it to visually join
  // with the toolbar.  This painting can't be done in the NonClientFrameView
  // because parts of the BrowserView (such as tabs) would get rendered on top
  // of the stroke.  It can't be done in BrowserView either because that view is
  // offset from the widget by a few DIPs, which is troublesome for computing a
  // subpixel offset when using fractional scale factors.  So we're forced to
  // put this drawing in the BrowserRootView.
  if (tabstrip()->ShouldDrawStrokes() && browser_view_->IsToolbarVisible()) {
    ui::PaintRecorder recorder(paint_info.context(),
                               paint_info.paint_recording_size(),
                               paint_info.paint_recording_scale_x(),
                               paint_info.paint_recording_scale_y(), nullptr);
    gfx::Canvas* canvas = recorder.canvas();

    const float scale = canvas->image_scale();

    gfx::RectF toolbar_bounds(browser_view_->toolbar()->bounds());
    ConvertRectToTarget(browser_view_, this, &toolbar_bounds);
    const int bottom = std::round(toolbar_bounds.y() * scale);
    const int x = std::round(toolbar_bounds.x() * scale);
    const int width = std::round(toolbar_bounds.width() * scale);

    gfx::ScopedCanvas scoped_canvas(canvas);
    const std::optional<int> active_tab_index = tabstrip()->GetActiveIndex();
    if (active_tab_index.has_value()) {
      Tab* active_tab = tabstrip()->tab_at(active_tab_index.value());
      if (active_tab && active_tab->GetVisible()) {
        gfx::RectF bounds(active_tab->GetMirroredBounds());
        // The root of the views tree that hosts tabstrip is BrowserRootView.
        // Except in Mac Immersive Fullscreen where the tabstrip is hosted in
        // `overlay_widget` or `tab_overlay_widget`, each have their own root
        // view.
        ConvertRectToTarget(tabstrip(), tabstrip()->GetWidget()->GetRootView(),
                            &bounds);
        canvas->ClipRect(bounds, SkClipOp::kDifference);
      }
    }
    canvas->UndoDeviceScaleFactor();

    const auto* widget = GetWidget();
    DCHECK(widget);
    const SkColor toolbar_top_separator_color =
        widget->GetColorProvider()->GetColor(
            GetWidget()->ShouldPaintAsActive()
                ? kColorToolbarTopSeparatorFrameActive
                : kColorToolbarTopSeparatorFrameInactive);

    cc::PaintFlags flags;
    flags.setColor(toolbar_top_separator_color);
    flags.setAntiAlias(true);
    const float stroke_width = scale;
    // Outset the rectangle and corner radius by half the stroke width
    // to draw an outer stroke.
    const float stroke_outset = stroke_width / 2;
    const float corner_radius =
        GetLayoutConstant(TOOLBAR_CORNER_RADIUS) * scale + stroke_outset;

    flags.setStyle(cc::PaintFlags::kStroke_Style);
    flags.setStrokeWidth(stroke_width);

    // Only draw the top half of the rounded rect.
    canvas->ClipRect(gfx::RectF(x, 0, width, bottom + corner_radius),
                     SkClipOp::kIntersect);

    gfx::RectF rect(x, bottom, width, 2 * corner_radius);
    rect.Outset(stroke_outset);
    canvas->DrawRoundRect(rect, corner_radius, flags);
  }
}

BrowserRootView::DropTarget* BrowserRootView::GetDropTarget(
    const ui::DropTargetEvent& event) {
  BrowserRootView::DropTarget* target = nullptr;

  // See if we should drop links onto tabstrip first.
  gfx::Point loc_in_tabstrip(event.location());
  ConvertPointToTarget(this, tabstrip(), &loc_in_tabstrip);
  target = tabstrip()->GetDropTarget(loc_in_tabstrip);

  // See if we can drop links onto toolbar.
  if (!target) {
    gfx::Point loc_in_toolbar(event.location());
    ConvertPointToTarget(this, toolbar(), &loc_in_toolbar);
    target =
        static_cast<BrowserRootView::DropTarget*>(toolbar())->GetDropTarget(
            loc_in_toolbar);
  }

  return target;
}

std::optional<BrowserRootView::DropIndex> BrowserRootView::GetDropIndexForEvent(
    const ui::DropTargetEvent& event,
    const ui::OSExchangeData& data,
    DropTarget* target) {
  gfx::Point loc_in_view(event.location());
  ConvertPointToTarget(this, target->GetViewForDrop(), &loc_in_view);
  ui::DropTargetEvent event_in_view(data, gfx::PointF(loc_in_view),
                                    gfx::PointF(loc_in_view),
                                    event.source_operations());
  return target->GetDropIndex(event_in_view);
}

void BrowserRootView::OnFilteringComplete(int sequence,
                                          std::vector<GURL> urls) {
  if (drop_info_ && drop_info_->sequence == sequence) {
    drop_info_->urls = std::move(urls);
    drop_info_->filtering_complete = true;
  }

  if (on_filtering_complete_closure_) {
    std::move(on_filtering_complete_closure_).Run();
  }
}

void BrowserRootView::SetOnFilteringCompleteClosureForTesting(
    base::OnceClosure closure) {
  on_filtering_complete_closure_ = std::move(closure);
}

std::optional<GURL> BrowserRootView::GetPasteAndGoURL(
    const ui::OSExchangeData& data) {
  std::optional<std::u16string> text_result = data.GetString();
  if (!text_result.has_value() || text_result->empty()) {
    return std::nullopt;
  }
  std::u16string text = AutocompleteMatch::SanitizeString(*text_result);

  AutocompleteMatch match;
  AutocompleteClassifierFactory::GetForProfile(
      browser_view_->browser()->profile())
      ->Classify(text, false, false, metrics::OmniboxEventProto::INVALID_SPEC,
                 &match, nullptr);
  if (!match.destination_url.is_valid()) {
    return std::nullopt;
  }

  return match.destination_url;
}

void BrowserRootView::NavigateToDroppedUrls(
    std::unique_ptr<DropInfo> drop_info,
    const ui::DropTargetEvent& event,
    ui::mojom::DragOperation& output_drag_op,
    std::unique_ptr<ui::LayerTreeOwner> drag_image_layer_owner) {
  DCHECK(drop_info);

  Browser* const browser = browser_view_->browser();
  TabStripModel* const model = browser->tab_strip_model();

  // If the browser window is not visible, it's about to be destroyed.
  if (!browser->window()->IsVisible() || model->empty()) {
    return;
  }

  // If there is no index then the target declined the drop.
  if (!drop_info->index.has_value()) {
    return;
  }

  // If the insertion point is off the end of the actual tab count, something
  // went wrong between when the drop was calculated and now. Bail.
  if (drop_info->index->index > model->GetTabCount()) {
    return;
  }

  // To handle the four permutations of (one URL, multiple URLs) × (insert
  // between tabs, replace tab), process the dropped URLs in two phases.
  //
  // Phase one: If the drop is indicated to replace the specified tab, then
  // replace the tab with the first URL of the drop. Remove the first URL from
  // the list of dropped URLs. Otherwise, skip this phase.
  //
  // Phase two: Create one tab for each remaining dropped URL, in reverse order.
  // This preserves the ordering of the dropped URLs.

  base::span<GURL> urls(drop_info->urls);
  CHECK(!urls.empty());
  int insertion_index = drop_info->index->index;

  if (drop_info->index->relative_to_index ==
      DropIndex::RelativeToIndex::kReplaceIndex) {
    NavigateParams params(browser_view_->browser(), urls[0],
                          ui::PAGE_TRANSITION_LINK);
    params.tabstrip_index = insertion_index;
    base::RecordAction(base::UserMetricsAction("Tab_DropURLOnTab"));
    params.disposition = WindowOpenDisposition::CURRENT_TAB;
    params.source_contents = model->GetWebContentsAt(insertion_index);
    params.window_action = NavigateParams::SHOW_WINDOW;
    Navigate(&params);

    urls = urls.subspan(1);
    ++insertion_index;  // Additional URLs inserted to the right.
  }

  for (const GURL& url : base::Reversed(urls)) {
    NavigateParams params(browser_view_->browser(), url,
                          ui::PAGE_TRANSITION_LINK);
    params.tabstrip_index = insertion_index;
    base::RecordAction(base::UserMetricsAction("Tab_DropURLBetweenTabs"));
    params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
    if (drop_info->index->group_inclusion ==
            DropIndex::GroupInclusion::kIncludeInGroup &&
        insertion_index < model->count()) {
      params.group = model->GetTabGroupForTab(insertion_index);
    }
    params.window_action = NavigateParams::SHOW_WINDOW;
    Navigate(&params);
  }

  // Ensure that the leftmost affected tab is the active one. If this drop was
  // insertion-only, then the URLs were inserted right-to-left, leaving the
  // leftmost tab active. If this was a replacement, then after the insertion of
  // the remainder of the tabs, the second-to-the-left-most tab is active, which
  // is odd, so manually select the leftmost tab.
  if (drop_info->index->relative_to_index ==
      DropIndex::RelativeToIndex::kReplaceIndex) {
    model->ActivateTabAt(drop_info->index->index);
  }

  output_drag_op = GetDropEffect(event);
}

BEGIN_METADATA(BrowserRootView)
END_METADATA
