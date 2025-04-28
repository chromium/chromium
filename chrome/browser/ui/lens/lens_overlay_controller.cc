// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/process/kill.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/feedback/show_feedback_page.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_event_handler.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_languages_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "chrome/browser/ui/lens/lens_overlay_untrusted_ui.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/lens_permission_bubble_controller.h"
#include "chrome/browser/ui/lens/lens_preselection_bubble.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/page_content_type_conversions.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/lens/lens_overlay_side_panel_result.h"
#include "components/omnibox/browser/lens_suggest_inputs_utils.h"
#include "components/optimization_guide/content/browser/page_content_proto_provider.h"
#include "components/permissions/permission_request_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_ui.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "pdf/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/native_widget.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/mojom/pdf.mojom.h"
#endif  // BUILDFLAG(ENABLE_PDF)

void* kLensOverlayPreselectionWidgetIdentifier =
    &kLensOverlayPreselectionWidgetIdentifier;

namespace {

// Timeout for the fadeout animation. This is purposely set to be twice the
// duration of the fade out animation on the WebUI JS because there is a delay
// between us notifying the WebUI, and the WebUI receiving our event.
constexpr base::TimeDelta kFadeoutAnimationTimeout = base::Milliseconds(300);

// The amount of time to wait for a reflow after closing the side panel before
// taking a screenshot.
constexpr base::TimeDelta kReflowWaitTimeout = base::Milliseconds(200);

// The amount of change in bytes that is considered a significant change and
// should trigger a page content update request. This provides tolerance in
// case there is slight variation in the retrievied bytes in between calls.
constexpr float kByteChangeTolerancePercent = 0.01;

// The maximum length of the DOM text to consider for OCR similarity.
// Currently 50 MB
constexpr int kMaxDomTextLengthForOcrSimilarity = 50 * 1000 * 1000;

// The url query param key for the search query.
inline constexpr char kTextQueryParameterKey[] = "q";

// Copy the objects of a vector into another without transferring
// ownership.
std::vector<lens::mojom::OverlayObjectPtr> CopyObjects(
    const std::vector<lens::mojom::OverlayObjectPtr>& objects) {
  std::vector<lens::mojom::OverlayObjectPtr> objects_copy(objects.size());
  std::transform(
      objects.begin(), objects.end(), objects_copy.begin(),
      [](const lens::mojom::OverlayObjectPtr& obj) { return obj->Clone(); });
  return objects_copy;
}

// Given a BGR bitmap, converts into a RGB bitmap instead. Returns empty bitmap
// if creation fails.
SkBitmap CreateRgbBitmap(const SkBitmap& bgr_bitmap) {
  // Convert bitmap from color type `kBGRA_8888_SkColorType` into a new Bitmap
  // with color type `kRGBA_8888_SkColorType` which will allow the bitmap to
  // render properly in the WebUI.
  sk_sp<SkColorSpace> srgb_color_space =
      bgr_bitmap.colorSpace()->makeSRGBGamma();
  SkImageInfo rgb_info = bgr_bitmap.info()
                             .makeColorType(kRGBA_8888_SkColorType)
                             .makeColorSpace(SkColorSpace::MakeSRGB());
  SkBitmap rgb_bitmap;
  rgb_bitmap.setInfo(rgb_info);
  rgb_bitmap.allocPixels(rgb_info);
  if (rgb_bitmap.writePixels(bgr_bitmap.pixmap())) {
    return rgb_bitmap;
  }

  // Bitmap creation failed.
  return SkBitmap();
}

// Returns a new string with all non-alphanumeric characters removed from the
// ends of the string.
std::string TrimNonAlphaNumeric(const std::string& text) {
  if (text.empty()) {
    return text;
  }

  // Find the first alphanumeric character from the beginning.
  size_t first_alphanum_index =
      std::find_if(text.begin(), text.end(), ::isalnum) - text.begin();

  // If no alphanumeric character is found in the entire string, return an empty
  // string.
  if (first_alphanum_index == text.length()) {
    return "";
  }

  // Find the index of the last alphanumeric character from the end.
  size_t last_alphanum_index =
      std::find_if(text.rbegin(), text.rend(), ::isalnum) - text.rbegin();
  // `last_alphanumeric` is the count from the end of the string, so convert to
  // index from the beginning.
  last_alphanum_index = text.length() - 1 - last_alphanum_index;

  // Extract the substring containing only the alphanumeric characters and those
  // in between.
  return text.substr(first_alphanum_index,
                     last_alphanum_index - first_alphanum_index + 1);
}

// Returns the percentage of words in the OCR text that are also in the DOM
// text.
double CalculateWordOverlapSimilarity(std::string dom_text,
                                      lens::mojom::TextPtr ocr_text) {
  // Split dom_text into possible words.
  std::vector<std::string> dom_words = base::SplitString(
      dom_text, " \t\r\n<>", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Convert dom_text to lowercase, alphanumeric only map for comparison. The
  // map value is the number of times the word appears in the dom text.
  std::map<std::string, int> dom_words_map;
  for (std::string& word : dom_words) {
    std::string processed_word = TrimNonAlphaNumeric(base::ToLowerASCII(word));
    if (!processed_word.empty()) {
      dom_words_map[processed_word]++;
    }
  }

  // Count the number of words in ocr_text that are also in the dom text.
  double overlap_count = 0;
  double total_ocr_words = 0;
  if (ocr_text && ocr_text->text_layout &&
      ocr_text->text_layout->paragraphs.size() > 0) {
    for (const auto& paragraph : ocr_text->text_layout->paragraphs) {
      if (paragraph && paragraph->lines.size() > 0) {
        for (const auto& line : paragraph->lines) {
          if (line && line->words.size() > 0) {
            for (const auto& word : line->words) {
              if (word) {
                std::string processed_word =
                    TrimNonAlphaNumeric(base::ToLowerASCII(word->plain_text));
                if (processed_word.empty()) {
                  continue;
                }

                // Find the process word in the dom words.
                auto word_iterator = dom_words_map.find(processed_word);
                if (word_iterator != dom_words_map.end() &&
                    word_iterator->second > 0) {
                  // The word is in the dom text.
                  overlap_count++;

                  // Decrement the count in the map so if there are multiple of
                  // this word in the DOM, we only count it for each instance.
                  word_iterator->second--;
                }
                total_ocr_words++;
              }
            }
          }
        }
      }
    }
  }

  // Avoid divide by zero. Return the percentage of words in the OCR text that
  // are also in the DOM text.
  return total_ocr_words == 0 ? 0.0 : overlap_count / total_ocr_words;
}

// Converts a JSON string array to a vector.
std::vector<std::string> JSONArrayToVector(const std::string& json_array) {
  std::optional<base::Value> json_value = base::JSONReader::Read(json_array);

  if (!json_value) {
    return {};
  }

  base::Value::List* entries = json_value->GetIfList();
  if (!entries) {
    return {};
  }

  std::vector<std::string> result;
  result.reserve(entries->size());
  for (const base::Value& entry : *entries) {
    const std::string* filter = entry.GetIfString();
    if (filter) {
      result.emplace_back(*filter);
    }
  }
  return result;
}

LensOverlayController* GetLensOverlayControllerFromTabInterface(
    tabs::TabInterface* tab_interface) {
  return tab_interface
             ? tab_interface->GetTabFeatures()->lens_overlay_controller()
             : nullptr;
}

}  // namespace

LensOverlayController::LensOverlayController(
    tabs::TabInterface* tab,
    LensSearchController* lens_search_controller,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    ThemeService* theme_service)
    : tab_(tab),
      lens_search_controller_(lens_search_controller),
      variations_client_(variations_client),
      identity_manager_(identity_manager),
      pref_service_(pref_service),
      sync_service_(sync_service),
      theme_service_(theme_service),
      gen204_controller_(
          std::make_unique<lens::LensOverlayGen204Controller>()) {
  tab_subscriptions_.push_back(tab_->RegisterDidActivate(base::BindRepeating(
      &LensOverlayController::TabForegrounded, weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDeactivate(
      base::BindRepeating(&LensOverlayController::TabWillEnterBackground,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDiscardContents(
      base::BindRepeating(&LensOverlayController::WillDiscardContents,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDetach(base::BindRepeating(
      &LensOverlayController::WillDetach, weak_factory_.GetWeakPtr())));
  lens_overlay_event_handler_ =
      std::make_unique<lens::LensOverlayEventHandler>(this);

  InitializeTutorialIPHUrlMatcher();

  // Listen to WebContents events
  tab_contents_observer_ = std::make_unique<UnderlyingWebContentsObserver>(
      tab_->GetContents(), this);
}

LensOverlayController::~LensOverlayController() {
  tab_contents_observer_.reset();
  state_ = State::kOff;
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(LensOverlayController, kOverlayId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(LensOverlayController,
                                      kOverlaySidePanelWebViewId);

// static.
LensOverlayController* LensOverlayController::FromWebUIWebContents(
    content::WebContents* webui_web_contents) {
  return GetLensOverlayControllerFromTabInterface(
      webui::GetTabInterface(webui_web_contents));
}

// static.
LensOverlayController* LensOverlayController::FromTabWebContents(
    content::WebContents* tab_web_contents) {
  return GetLensOverlayControllerFromTabInterface(
      tabs::TabInterface::GetFromContents(tab_web_contents));
}

void LensOverlayController::IssueContextualSearchRequest(
    const GURL& destination_url,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion) {
  // Ignore the request if the overlay is off or closing.
  if (IsOverlayClosing()) {
    return;
  }

  // If the overlay is off, turn it on so the request can be fulfilled.
  if (state_ == State::kOff) {
    // TODO(crbug.com/402497756): For prototyping, reusing the existing
    // omnibox entry point. However, for production, create a new invocation
    // source for this new entry point.
    // TODO(crbug.com/403573362): This is a temporary fix to unblock
    // prototyping. Since this flow goes straight to the side panel results with
    // not overlay UI, this flow does a lot of unnecessary work. There should be
    // a new flow that can contextualize without the overlay UI being
    // initialized.
    StartContextualizationWithoutOverlay(
        lens::LensOverlayInvocationSource::kOmnibox);
  }

  // Hold the request until the overlay has finished initializing.
  if (IsOverlayInitializing()) {
    pending_contextual_search_request_ =
        base::BindOnce(&LensOverlayController::IssueContextualSearchRequest,
                       weak_factory_.GetWeakPtr(), destination_url, match_type,
                       is_zero_prefix_suggestion);
    return;
  }

  // TODO(crbug.com/401583049): Revisit if this should go through the
  // OnSuggestionAccepted flow or if there should be a more direct contextual
  // search flow.
  OnSuggestionAccepted(destination_url, match_type, is_zero_prefix_suggestion);
}

void LensOverlayController::StartContextualizationWithoutOverlay(
    lens::LensOverlayInvocationSource invocation_source) {
  should_show_overlay_ = false;
  ShowUI(invocation_source);
}

void LensOverlayController::ShowUIWithPendingRegion(
    lens::LensOverlayInvocationSource invocation_source,
    const gfx::Rect& tab_bounds,
    const gfx::Rect& view_bounds,
    const gfx::Rect& image_bounds,
    const SkBitmap& region_bitmap) {
  ShowUIWithPendingRegion(invocation_source,
                          lens::GetCenterRotatedBoxFromTabViewAndImageBounds(
                              tab_bounds, view_bounds, image_bounds),
                          region_bitmap);
}

void LensOverlayController::ShowUIWithPendingRegion(
    lens::LensOverlayInvocationSource invocation_source,
    lens::mojom::CenterRotatedBoxPtr region,
    const SkBitmap& region_bitmap) {
  pending_region_ = std::move(region);
  pending_region_bitmap_ = region_bitmap;
  ShowUI(invocation_source);
  // Overrides value set in ShowUI since invoking lens overlay with a pending
  // region is considered a search.
  search_performed_in_session_ = true;
}

void LensOverlayController::ShowUI(
    lens::LensOverlayInvocationSource invocation_source) {
  // If UI is already showing or in the process of showing, do nothing.
  if (state_ != State::kOff) {
    return;
  }

  // The UI should only show if the tab is in the foreground or if the tab web
  // contents is not in a crash state.
  if (!tab_->IsActivated() || tab_->GetContents()->IsCrashed()) {
    return;
  }

  // If a different tab-modal is showing, do nothing.
  if (!tab_->CanShowModalUI()) {
    return;
  }

  invocation_source_ = invocation_source;

  // Request user permission before grabbing a screenshot.
  CHECK(pref_service_);
  // If contextual serachbox is enabled, show permission bubble again informing
  // users of other information that will be shared. The contextual searchbox
  // pref is a different pref.
  if (!lens::CanSharePageScreenshotWithLensOverlay(pref_service_) ||
      (lens::features::IsLensOverlayContextualSearchboxEnabled() &&
       !lens::CanSharePageContentWithLensOverlay(pref_service_))) {
    if (!permission_bubble_controller_) {
      permission_bubble_controller_ =
          std::make_unique<lens::LensPermissionBubbleController>(
              *tab_, pref_service_, invocation_source);
    }
    permission_bubble_controller_->RequestPermission(
        tab_->GetContents(),
        base::BindRepeating(&LensOverlayController::ShowUI,
                            weak_factory_.GetWeakPtr(), invocation_source));
    return;
  }

  // Increment the counter for the number of times the Lens Overlay has been
  // started.
  int lens_overlay_start_count =
      pref_service_->GetInteger(prefs::kLensOverlayStartCount);
  pref_service_->SetInteger(prefs::kLensOverlayStartCount,
                            lens_overlay_start_count + 1);

  // Grab reference to the side panel coordinator it not already done so.
  if (!results_side_panel_coordinator_) {
    results_side_panel_coordinator_ =
        lens_search_controller_->lens_overlay_side_panel_coordinator();
  }

  Profile* profile =
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext());
  // Create the query controller.
  lens_overlay_query_controller_ = CreateLensQueryController(
      base::BindRepeating(&LensOverlayController::HandleStartQueryResponse,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&LensOverlayController::HandleInteractionURLResponse,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&LensOverlayController::HandleInteractionResponse,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&LensOverlayController::HandleSuggestInputsResponse,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&LensOverlayController::HandleThumbnailCreated,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(
          &LensOverlayController::HandlePageContentUploadProgress,
          weak_factory_.GetWeakPtr()),
      variations_client_, identity_manager_, profile, invocation_source,
      lens::LensOverlayShouldUseDarkMode(theme_service_),
      gen204_controller_.get());
  side_panel_coordinator_ =
      tab_->GetBrowserWindowInterface()->GetFeatures().side_panel_coordinator();
  CHECK(side_panel_coordinator_);

  // Create the languages controller.
  languages_controller_ =
      std::make_unique<lens::LensOverlayLanguagesController>(profile);

  // Setup observer to be notified of side panel opens and closes.
  side_panel_shown_subscription_ =
      side_panel_coordinator_->RegisterSidePanelShown(
          base::BindRepeating(&LensOverlayController::OnSidePanelDidOpen,
                              weak_factory_.GetWeakPtr()));

  if (find_in_page::FindTabHelper* const find_tab_helper =
          find_in_page::FindTabHelper::FromWebContents(tab_->GetContents())) {
    find_tab_observer_.Observe(find_tab_helper);
  }

  if (!omnibox_tab_helper_observer_.IsObserving()) {
    if (auto* helper = OmniboxTabHelper::FromWebContents(tab_->GetContents())) {
      omnibox_tab_helper_observer_.Observe(helper);
    }
  }

  // This is safe because we checked if another modal was showing above.
  scoped_tab_modal_ui_ = tab_->ShowModalUI();
  fullscreen_observation_.Observe(tab_->GetBrowserWindowInterface()
                                      ->GetExclusiveAccessManager()
                                      ->fullscreen_controller());

  // The preselection widget can cover top Chrome in immersive fullscreen.
  // Observer the reveal state to hide the widget when top Chrome is shown.
  immersive_mode_observer_.Observe(
      tab_->GetBrowserWindowInterface()->GetImmersiveModeController());

#if BUILDFLAG(IS_MAC)
  // Add observer to listen for changes in the always show toolbar state,
  // since that requires the preselection bubble to rerender to show properly.
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kShowFullscreenToolbar,
      base::BindRepeating(
          &LensOverlayController::CloseAndReshowPreselectionBubble,
          base::Unretained(this)));
#endif  // BUILDFLAG(IS_MAC)

  NotifyUserEducationAboutOverlayUsed();

  // Establish data required for session metrics.
  search_performed_in_session_ = false;
  invocation_time_ = base::TimeTicks::Now();
  invocation_time_since_epoch_ = base::Time::Now();
  hats_triggered_in_session_ = false;
  ocr_dom_similarity_recorded_in_session_ = false;

  // This should be the last thing called in ShowUI, so if something goes wrong
  // in capturing the screenshot, the state gets cleaned up correctly.
  if (side_panel_coordinator_->IsSidePanelShowing()) {
    // Close the currently opened side panel synchronously. Postpone the
    // screenshot for a fixed time to allow reflow.
    state_ = State::kClosingOpenedSidePanel;
    side_panel_coordinator_->Close(/*suppress_animations=*/true);
    base::SingleThreadTaskRunner::GetCurrentDefault()
        ->PostNonNestableDelayedTask(
            FROM_HERE,
            base::BindOnce(&LensOverlayController::FinishedWaitingForReflow,
                           weak_factory_.GetWeakPtr()),
            kReflowWaitTimeout);
  } else {
    CaptureScreenshot();
  }
}

void LensOverlayController::CloseUIAsync(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (state_ == State::kOff || IsOverlayClosing()) {
    return;
  }

  // Notify the overlay so it can do any animations or cleanup. The page_ is not
  // guaranteed to exist if CloseUIAsync is called during the setup process.
  if (page_) {
    page_->NotifyOverlayClosing();
  }

  if (state_ == State::kOverlayAndResults ||
      state_ == State::kLivePageAndResults) {
    if (side_panel_coordinator_->GetCurrentEntryId() ==
        SidePanelEntry::Id::kLensOverlayResults) {
      // If a close was triggered while our side panel is showing, instead of
      // just immediately closing the overlay, we close side panel to show a
      // smooth closing animation. Once the side panel deregisters, it will
      // re-call our close method in OnSidePanelDidClose() which will finish the
      // closing process.
      state_ = State::kClosingSidePanel;
      last_dismissal_source_ = dismissal_source;
      side_panel_coordinator_->Close();
      return;
    }
  }

  state_ = State::kClosing;
  // Set a short 200ms timeout to give the fade out time to transition.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LensOverlayController::CloseUIPart2,
                     weak_factory_.GetWeakPtr(), dismissal_source),
      kFadeoutAnimationTimeout);
}

void LensOverlayController::CloseUISync(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (state_ == State::kOff) {
    return;
  }

  state_ = State::kClosing;
  CloseUIPart2(dismissal_source);
}

// static
const std::u16string LensOverlayController::GetFilenameForURL(const GURL& url) {
  if (!url.has_host() || url.HostIsIPAddress()) {
    return u"screenshot.png";
  }

  return base::ASCIIToUTF16(base::StrCat({"screenshot_", url.host(), ".png"}));
}

void LensOverlayController::BindOverlay(
    mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensPage> page) {
  if (state_ != State::kStartingWebUI) {
    return;
  }
  receiver_.Bind(std::move(receiver));
  page_.Bind(std::move(page));

  InitializeOverlay(/*initialization_data=*/nullptr);
}

void LensOverlayController::BindOverlayGhostLoader(
    mojo::PendingRemote<lens::mojom::LensGhostLoaderPage> page) {
  overlay_ghost_loader_page_.reset();
  overlay_ghost_loader_page_.Bind(std::move(page));
}

void LensOverlayController::BindSidePanelGhostLoader(
    mojo::PendingRemote<lens::mojom::LensGhostLoaderPage> page) {
  side_panel_ghost_loader_page_.reset();
  side_panel_ghost_loader_page_.Bind(std::move(page));
}

void LensOverlayController::SetSidePanelSearchboxHandler(
    std::unique_ptr<LensSearchboxHandler> handler) {
  side_panel_searchbox_handler_ = std::move(handler);
}

void LensOverlayController::SetContextualSearchboxHandler(
    std::unique_ptr<LensSearchboxHandler> handler) {
  overlay_searchbox_handler_ = std::move(handler);
}

void LensOverlayController::ResetSidePanelSearchboxHandler() {
  side_panel_searchbox_handler_.reset();
}

uint64_t LensOverlayController::GetInvocationTimeSinceEpoch() {
  return invocation_time_since_epoch_.InMillisecondsSinceUnixEpoch();
}

views::View* LensOverlayController::GetOverlayViewForTesting() {
  return overlay_view_.get();
}

views::WebView* LensOverlayController::GetOverlayWebViewForTesting() {
  return overlay_web_view_.get();
}

void LensOverlayController::SendText(lens::mojom::TextPtr text) {
  if (!page_) {
    // Store the text to send once the page is bound.
    pre_initialization_text_ = std::move(text);
    return;
  }
  page_->TextReceived(std::move(text));
}

lens::mojom::OverlayThemePtr LensOverlayController::CreateTheme(
    lens::PaletteId palette_id) {
  CHECK(base::Contains(lens::kPaletteColors, palette_id));
  const auto& palette = lens::kPaletteColors.at(palette_id);
  auto theme = lens::mojom::OverlayTheme::New();
  theme->primary = palette.at(lens::ColorId::kPrimary);
  theme->shader_layer_1 = palette.at(lens::ColorId::kShaderLayer1);
  theme->shader_layer_2 = palette.at(lens::ColorId::kShaderLayer2);
  theme->shader_layer_3 = palette.at(lens::ColorId::kShaderLayer3);
  theme->shader_layer_4 = palette.at(lens::ColorId::kShaderLayer4);
  theme->shader_layer_5 = palette.at(lens::ColorId::kShaderLayer5);
  theme->scrim = palette.at(lens::ColorId::kScrim);
  theme->surface_container_highest_light =
      palette.at(lens::ColorId::kSurfaceContainerHighestLight);
  theme->surface_container_highest_dark =
      palette.at(lens::ColorId::kSurfaceContainerHighestDark);
  theme->selection_element = palette.at(lens::ColorId::kSelectionElement);
  return theme;
}

void LensOverlayController::SendObjects(
    std::vector<lens::mojom::OverlayObjectPtr> objects) {
  if (!page_) {
    // Store the objects to send once the page is bound.
    pre_initialization_objects_ = std::move(objects);
    return;
  }
  page_->ObjectsReceived(std::move(objects));
}

void LensOverlayController::NotifyResultsPanelOpened() {
  page_->NotifyResultsPanelOpened();
}

void LensOverlayController::TriggerCopy() {
  // This prevents a race condition where the overlay is closed as a keyboard
  // event is being processed.
  if (!page_) {
    return;
  }
  page_->OnCopyCommand();
}

bool LensOverlayController::IsOverlayShowing() {
  return state_ == State::kStartingWebUI || state_ == State::kOverlay ||
         state_ == State::kOverlayAndResults ||
         state_ == State::kClosingSidePanel;
}

bool LensOverlayController::IsOverlayActive() {
  return IsOverlayShowing() || state_ == State::kLivePageAndResults;
}

bool LensOverlayController::IsOverlayInitializing() {
  return state_ == State::kStartingWebUI || state_ == State::kScreenshot ||
         state_ == State::kClosingOpenedSidePanel;
}

bool LensOverlayController::IsOverlayClosing() {
  return state_ == State::kClosing || state_ == State::kClosingSidePanel;
}

bool LensOverlayController::IsContextualSearchbox() {
  // TODO(crbug.com/405441183): This logic will break the side panel searchbox
  // if there is no overlay, so it should be moved to a shared location.
  return GetPageClassification() ==
         metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX;
}

void LensOverlayController::GetIsContextualSearchbox(
    GetIsContextualSearchboxCallback callback) {
  std::move(callback).Run(IsContextualSearchbox());
}

bool LensOverlayController::IsScreenshotPossible(
    content::RenderWidgetHostView* view) {
  return view && view->IsSurfaceAvailableForCopy();
}

void LensOverlayController::OnSidePanelWillHide(
    SidePanelEntryHideReason reason) {
  // If the tab is not in the foreground, this is not relevant.
  if (!tab_->IsActivated()) {
    return;
  }

  if (!IsOverlayClosing()) {
    if (reason == SidePanelEntryHideReason::kReplaced) {
      // If the Lens side panel is being replaced, don't close the side panel.
      // Instead, set the state and dismissal source and wait for
      // OnSidePanelHidden to be called.
      state_ = State::kClosingSidePanel;
      last_dismissal_source_ =
          lens::LensOverlayDismissalSource::kSidePanelEntryReplaced;
    } else {
      // Trigger the close animation and notify the overlay that the side
      // panel is closing so that it can fade out the UI.
      CloseUIAsync(lens::LensOverlayDismissalSource::kSidePanelCloseButton);
    }
  }
}

void LensOverlayController::OnSidePanelHidden() {
  if (state_ != State::kClosingSidePanel) {
    return;
  }
  CHECK(last_dismissal_source_.has_value());
  CloseUIPart2(*last_dismissal_source_);
  last_dismissal_source_.reset();
}

tabs::TabInterface* LensOverlayController::GetTabInterface() {
  return tab_;
}

void LensOverlayController::IssueLensRegionRequestForTesting(
    lens::mojom::CenterRotatedBoxPtr region,
    bool is_click) {
  IssueLensRegionRequest(std::move(region), is_click);
}

void LensOverlayController::IssueTextSelectionRequestForTesting(
    const std::string& text_query,
    int selection_start_index,
    int selection_end_index,
    bool is_translate) {
  IssueTextSelectionRequest(text_query, selection_start_index,
                            selection_end_index, is_translate);
}

void LensOverlayController::
    RecordUkmAndTaskCompletionForLensOverlayInteractionForTesting(
        lens::mojom::UserAction user_action) {
  RecordUkmAndTaskCompletionForLensOverlayInteraction(user_action);
}

void LensOverlayController::RecordSemanticEventForTesting(
    lens::mojom::SemanticEvent event) {
  RecordLensOverlaySemanticEvent(event);
}

void LensOverlayController::IssueSearchBoxRequestForTesting(
    const std::string& search_box_text,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion,
    std::map<std::string, std::string> additional_query_params) {
  IssueSearchBoxRequest(search_box_text, match_type, is_zero_prefix_suggestion,
                        additional_query_params);
}

void LensOverlayController::IssueTranslateSelectionRequestForTesting(
    const std::string& text_query,
    const std::string& content_language,
    int selection_start_index,
    int selection_end_index) {
  IssueTranslateSelectionRequest(text_query, content_language,
                                 selection_start_index, selection_end_index);
}

void LensOverlayController::IssueMathSelectionRequestForTesting(
    const std::string& query,
    const std::string& formula,
    int selection_start_index,
    int selection_end_index) {
  IssueMathSelectionRequest(query, formula, selection_start_index,
                            selection_end_index);
}

void LensOverlayController::IssueTranslateFullPageRequestForTesting(
    const std::string& source_language,
    const std::string& target_language) {
  IssueTranslateFullPageRequest(source_language, target_language);
}

void LensOverlayController::IssueEndTranslateModeRequestForTesting() {
  IssueEndTranslateModeRequest();
}

void LensOverlayController::IssueTranslateFullPageRequest(
    const std::string& source_language,
    const std::string& target_language) {
  // Remove the selection thumbnail, if it exists.
  SetSearchboxThumbnail(std::string());
  ClearRegionSelection();
  // Set the coachmark text.
  if (preselection_widget_) {
    // This cast is safe since we know the widget delegate will always be a
    // `lens::LensPreselectionBubble`.
    auto* bubble_view = static_cast<lens::LensPreselectionBubble*>(
        preselection_widget_->widget_delegate());
    bubble_view->SetLabelText(
        IDS_LENS_OVERLAY_INITIAL_TOAST_MESSAGE_SELECT_TEXT);
  }
  // Set the translate options on initialization data in case we need to
  // re-enable translate mode later.
  initialization_data_->translate_options_ =
      lens::TranslateOptions(source_language, target_language);

  lens_overlay_query_controller_->SendFullPageTranslateQuery(source_language,
                                                             target_language);
  MaybeLaunchSurvey();
}

void LensOverlayController::IssueEndTranslateModeRequest() {
  // Reset the coachmark text back to default.
  if (preselection_widget_) {
    // This cast is safe since we know the widget delegate will always be a
    // `lens::LensPreselectionBubble`.
    auto* bubble_view = static_cast<lens::LensPreselectionBubble*>(
        preselection_widget_->widget_delegate());
    bubble_view->SetLabelText(IDS_LENS_OVERLAY_INITIAL_TOAST_MESSAGE);
  }
  lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
  initialization_data_->selected_text_.reset();
  initialization_data_->translate_options_.reset();
  lens_overlay_query_controller_->SendEndTranslateModeQuery();
}

void LensOverlayController::NotifyOverlayInitialized() {
  // Now that the overlay is actually showing, it is safe to start doing a Lens
  // request without showing the page reflowing.
  if (pending_region_) {
    // If there is a pending region (i.e. for image right click)
    // use INJECTED_IMAGE as the selection type.
    IssueLensRequest(std::move(pending_region_), lens::INJECTED_IMAGE,
                     pending_region_bitmap_);
    pending_region_bitmap_.reset();
  }
}

void LensOverlayController::CopyText(const std::string& text) {
  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
  clipboard_writer.WriteText(base::UTF8ToUTF16(text));
}

void LensOverlayController::CopyImage(lens::mojom::CenterRotatedBoxPtr region) {
  SkBitmap cropped = lens::CropBitmapToRegion(
      initialization_data_->initial_screenshot_, std::move(region));
  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
  clipboard_writer.WriteImage(cropped);
}

void LensOverlayController::RecordUkmAndTaskCompletionForLensOverlayInteraction(
    lens::mojom::UserAction user_action) {
  ukm::SourceId source_id =
      tab_->GetContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  ukm::builders::Lens_Overlay_Overlay_UserAction(source_id)
      .SetUserAction(static_cast<int64_t>(user_action))
      .Record(ukm::UkmRecorder::Get());
  lens_overlay_query_controller_->SendTaskCompletionGen204IfEnabled(
      user_action);
}

void LensOverlayController::RecordLensOverlaySemanticEvent(
    lens::mojom::SemanticEvent event) {
  lens_overlay_query_controller_->SendSemanticEventGen204IfEnabled(event);
}

void LensOverlayController::SaveAsImage(
    lens::mojom::CenterRotatedBoxPtr region) {
  SkBitmap cropped = lens::CropBitmapToRegion(
      initialization_data_->initial_screenshot_, std::move(region));
  const GURL data_url = GURL(webui::GetBitmapDataUrl(cropped));
  content::DownloadManager* download_manager =
      tab_->GetBrowserWindowInterface()->GetProfile()->GetDownloadManager();
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("lens_overlay_save", R"(
      semantics {
        sender: "Lens Overlay"
        description:
          "The user may capture a selection of the current screenshot in the "
          "Lens overlay via a button in the overlay. The resulting image is "
          "saved from a data URL to the disk on the local client."
        trigger: "User clicks 'Save as image' in the Lens Overlay after "
           "activating the Lens Overlay and making a selection on the "
           "screenshot."
        data: "A capture of a portion of a screenshot of the current page."
        destination: LOCAL
        last_reviewed: "2024-08-23"
        user_data {
          type: WEB_CONTENT
        }
        internal {
          contacts {
            owners: "//chrome/browser/ui/lens/OWNERS"
          }
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "No user-visible setting for this feature. Configured via Finch."
        policy_exception_justification:
          "This is not a network request."
      })");
  std::unique_ptr<download::DownloadUrlParameters> params =
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          overlay_web_view_->GetWebContents(), data_url, traffic_annotation);
  params->set_prompt(true);
  params->set_suggested_name(
      GetFilenameForURL(tab_->GetContents()->GetLastCommittedURL()));
  download_manager->DownloadUrl(std::move(params));
}

void LensOverlayController::MaybeShowTranslateFeaturePromo() {
  auto* tracker = ui::ElementTracker::GetElementTracker();
  translate_button_shown_subscription_ =
      tracker->AddElementShownInAnyContextCallback(
          kLensOverlayTranslateButtonElementId,
          base::BindRepeating(
              &LensOverlayController::TryShowTranslateFeaturePromo,
              weak_factory_.GetWeakPtr()));
}

void LensOverlayController::MaybeCloseTranslateFeaturePromo(
    bool feature_engaged) {
  if (auto* const interface =
          BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
              tab_->GetContents())) {
    if (!interface->IsFeaturePromoActive(
            feature_engagement::kIPHLensOverlayTranslateButtonFeature)) {
      // Do nothing if feature promo is not active.
      return;
    }

    if (feature_engaged) {
      interface->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHLensOverlayTranslateButtonFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    } else {
      interface->AbortFeaturePromo(
          feature_engagement::kIPHLensOverlayTranslateButtonFeature);
    }
  }
}

void LensOverlayController::FetchSupportedLanguages(
    FetchSupportedLanguagesCallback callback) {
  CHECK(languages_controller_);
  languages_controller_->SendGetSupportedLanguagesRequest(std::move(callback));
}

void LensOverlayController::TryShowTranslateFeaturePromo(
    ui::TrackedElement* element) {
  if (!element) {
    return;
  }

  if (auto* const interface =
          BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
              tab_->GetContents())) {
    interface->MaybeShowFeaturePromo(
        feature_engagement::kIPHLensOverlayTranslateButtonFeature);
  }
}

std::string LensOverlayController::GetInvocationSourceString() {
  return lens::InvocationSourceToString(invocation_source_);
}

content::WebContents*
LensOverlayController::GetSidePanelWebContentsForTesting() {
  if (!results_side_panel_coordinator_) {
    return nullptr;
  }
  return results_side_panel_coordinator_->GetSidePanelWebContents();
}

const GURL& LensOverlayController::GetPageURLForTesting() {
  return GetPageURL();
}

SessionID LensOverlayController::GetTabIdForTesting() {
  return GetTabId();
}

metrics::OmniboxEventProto::PageClassification
LensOverlayController::GetPageClassificationForTesting() {
  return GetPageClassification();
}

const std::string& LensOverlayController::GetThumbnailForTesting() {
  return GetThumbnail();
}

void LensOverlayController::OnTextModifiedForTesting() {
  OnTextModified();
}

void LensOverlayController::OnThumbnailRemovedForTesting() {
  OnThumbnailRemoved();
}

void LensOverlayController::OnFocusChangedForTesting(bool focused) {
  OnFocusChanged(focused);
}

void LensOverlayController::OnZeroSuggestShownForTesting() {
  OnZeroSuggestShown();
}

void LensOverlayController::OpenSidePanelForTesting() {
  MaybeOpenSidePanel();
}

const lens::proto::LensOverlaySuggestInputs&
LensOverlayController::GetLensSuggestInputsForTesting() {
  return GetLensSuggestInputs();
}

bool LensOverlayController::IsUrlEligibleForTutorialIPHForTesting(
    const GURL& url) {
  return IsUrlEligibleForTutorialIPH(url);
}

std::unique_ptr<lens::LensOverlayQueryController>
LensOverlayController::CreateLensQueryController(
    lens::LensOverlayFullImageResponseCallback full_image_callback,
    lens::LensOverlayUrlResponseCallback url_callback,
    lens::LensOverlayInteractionResponseCallback interaction_callback,
    lens::LensOverlaySuggestInputsCallback suggest_inputs_callback,
    lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
    lens::UploadProgressCallback upload_progress_callback,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    Profile* profile,
    lens::LensOverlayInvocationSource invocation_source,
    bool use_dark_mode,
    lens::LensOverlayGen204Controller* gen204_controller) {
  return std::make_unique<lens::LensOverlayQueryController>(
      std::move(full_image_callback), std::move(url_callback),
      std::move(interaction_callback), std::move(suggest_inputs_callback),
      std::move(thumbnail_created_callback),
      std::move(upload_progress_callback), variations_client, identity_manager,
      profile, invocation_source, use_dark_mode, gen204_controller);
}

std::string LensOverlayController::GetVsridForNewTab() {
  return lens_overlay_query_controller_->GetVsridForNewTab();
}

void LensOverlayController::SetTranslateMode(
    std::optional<lens::TranslateOptions> translate_options) {
  if (translate_options.has_value()) {
    page_->SetTranslateMode(translate_options->source_language,
                            translate_options->target_language);
  } else {
    // If the overlay was previously in translate mode, send a
    // request to end translate mode so the WebUI can update its state.
    if (initialization_data_->translate_options_.has_value()) {
      IssueEndTranslateModeRequest();
      results_side_panel_coordinator_->SetSidePanelIsLoadingResults(true);
    }
    // Disable translate mode by setting source and target languages to empty
    // strings. This is a no-op if translate mode is already disabled.
    page_->SetTranslateMode(std::string(), std::string());
  }
  // Store the latest translate options.
  initialization_data_->translate_options_ = translate_options;
}

void LensOverlayController::SetTextSelection(int32_t selection_start_index,
                                             int32_t selection_end_index) {
  page_->SetTextSelection(selection_start_index, selection_end_index);
  initialization_data_->selected_text_ =
      std::make_pair(selection_start_index, selection_end_index);
}

void LensOverlayController::SetPostRegionSelection(
    lens::mojom::CenterRotatedBoxPtr box) {
  page_->SetPostRegionSelection(box->Clone());
  initialization_data_->selected_region_ = std::move(box);
}

void LensOverlayController::SetSearchboxInputText(const std::string& text) {
  if (side_panel_searchbox_handler_ &&
      side_panel_searchbox_handler_->IsRemoteBound()) {
    side_panel_searchbox_handler_->SetInputText(text);
  } else {
    // If the side panel was not bound at the time of request, we store the
    // query as pending to send it to the searchbox on bind.
    pending_text_query_ = text;
  }
}

void LensOverlayController::SetSearchboxThumbnail(
    const std::string& thumbnail_uri) {
  if (side_panel_searchbox_handler_ &&
      side_panel_searchbox_handler_->IsRemoteBound()) {
    side_panel_searchbox_handler_->SetThumbnail(thumbnail_uri);
    selected_region_thumbnail_uri_ = thumbnail_uri;
  } else {
    // If the side panel was not bound at the time of request, we store the
    // thumbnail as pending to send it to the searchbox on bind.
    pending_thumbnail_uri_ = thumbnail_uri;
  }
}

void LensOverlayController::SetAdditionalSearchQueryParams(
    std::map<std::string, std::string> additional_search_query_params) {
  initialization_data_->additional_search_query_params_ =
      additional_search_query_params;
}

void LensOverlayController::ClearAllSelections() {
  page_->ClearAllSelections();
  initialization_data_->selected_region_.reset();
  initialization_data_->selected_region_bitmap_.reset();
  initialization_data_->selected_text_.reset();
  if (!IsContextualSearchbox()) {
    lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
  }
}

void LensOverlayController::IssueLensRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    lens::LensOverlaySelectionType selection_type,
    std::optional<SkBitmap> region_bytes) {
  CHECK(initialization_data_);
  CHECK(region);
  SetSearchboxInputText(std::string());
  initialization_data_->selected_region_ = region.Clone();
  initialization_data_->selected_text_.reset();
  initialization_data_->additional_search_query_params_.clear();
  lens_selection_type_ = selection_type;
  if (region_bytes) {
    initialization_data_->selected_region_bitmap_ = region_bytes.value();
  } else {
    initialization_data_->selected_region_bitmap_.reset();
  }

  lens_overlay_query_controller_->SendRegionSearch(
      region.Clone(), selection_type,
      initialization_data_->additional_search_query_params_, region_bytes);
  MaybeOpenSidePanel();
  RecordTimeToFirstInteraction(
      lens::LensOverlayFirstInteractionType::kRegionSelect);
  state_ = State::kOverlayAndResults;
  MaybeLaunchSurvey();
}

void LensOverlayController::IssueMultimodalRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    const std::string& text_query,
    lens::LensOverlaySelectionType selection_type,
    std::optional<SkBitmap> region_bitmap) {
  lens_overlay_query_controller_->SendMultimodalRequest(
      std::move(region), text_query, selection_type,
      initialization_data_->additional_search_query_params_, region_bitmap);
}

void LensOverlayController::IssueContextualTextRequest(
    const std::string& text_query,
    lens::LensOverlaySelectionType selection_type) {
  lens_overlay_query_controller_->SendContextualTextQuery(
      text_query, selection_type,
      initialization_data_->additional_search_query_params_);
}

void LensOverlayController::AddOverlayStateToSearchQuery(
    lens::SearchQuery& search_query) {
  // In the case where a query was triggered by a selection on the overlay or
  // use of the searchbox, initialization_data_, additional_search_query_params_
  // and selected_region_thumbnail_uri_ will have already been set. Record
  // that state in a search query struct.
  if (initialization_data_->selected_region_) {
    search_query.selected_region_ =
        initialization_data_->selected_region_->Clone();
  }
  if (!initialization_data_->selected_region_bitmap_.drawsNothing()) {
    search_query.selected_region_bitmap_ =
        initialization_data_->selected_region_bitmap_;
  }
  search_query.selected_region_thumbnail_uri_ = selected_region_thumbnail_uri_;
  if (initialization_data_->selected_text_.has_value()) {
    search_query.selected_text_ = initialization_data_->selected_text_.value();
  }
  if (initialization_data_->translate_options_.has_value()) {
    search_query.translate_options_ =
        initialization_data_->translate_options_.value();
  }
  search_query.lens_selection_type_ = lens_selection_type_;
  search_query.additional_search_query_params_ =
      initialization_data_->additional_search_query_params_;
}

LensOverlayController::OverlayInitializationData::OverlayInitializationData(
    const SkBitmap& screenshot,
    SkBitmap rgb_screenshot,
    lens::PaletteId color_palette,
    GURL page_url,
    std::optional<std::string> page_title)
    : initial_screenshot_(screenshot),
      initial_rgb_screenshot_(std::move(rgb_screenshot)),
      updated_screenshot_(screenshot),
      color_palette_(color_palette),
      page_url_(page_url),
      page_title_(page_title) {}
LensOverlayController::OverlayInitializationData::~OverlayInitializationData() =
    default;

class LensOverlayController::UnderlyingWebContentsObserver
    : public content::WebContentsObserver {
 public:
  UnderlyingWebContentsObserver(content::WebContents* web_contents,
                                LensOverlayController* lens_overlay_controller)
      : content::WebContentsObserver(web_contents),
        lens_overlay_controller_(lens_overlay_controller) {}

  ~UnderlyingWebContentsObserver() override = default;

  UnderlyingWebContentsObserver(const UnderlyingWebContentsObserver&) = delete;
  UnderlyingWebContentsObserver& operator=(
      const UnderlyingWebContentsObserver&) = delete;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    // If the overlay is off, check if we should display IPH.
    if (lens_overlay_controller_->state() == State::kOff) {
      // Only check IPH eligibility if the navigation changed the primary page.
      if (base::FeatureList::IsEnabled(
              feature_engagement::kIPHLensOverlayFeature) &&
          navigation_handle->IsInPrimaryMainFrame() &&
          !navigation_handle->IsSameDocument() &&
          navigation_handle->HasCommitted()) {
        lens_overlay_controller_->MaybeShowDelayedTutorialIPH(
            navigation_handle->GetURL());
      }
      return;
    }

    // If the overlay is open, check if we should close it.
    bool is_user_reload =
        navigation_handle->GetReloadType() != content::ReloadType::NONE &&
        !navigation_handle->IsRendererInitiated();
    // We don't need to close if:
    //   1) The navigation is not for the main page.
    //   2) The navigation hasn't been committed yet.
    //   3) The URL did not change and the navigation wasn't the user reloading
    //      the page.
    if (!navigation_handle->IsInPrimaryMainFrame() ||
        !navigation_handle->HasCommitted() ||
        (navigation_handle->GetPreviousPrimaryMainFrameURL() ==
             navigation_handle->GetURL() &&
         !is_user_reload)) {
      return;
    }
    if (lens_overlay_controller_->state() == State::kLivePageAndResults) {
      lens_overlay_controller_->UpdateNavigationMetrics();
      lens_overlay_controller_->NotifyPageContentUpdated();
      return;
    }
    lens_overlay_controller_->CloseUISync(
        lens::LensOverlayDismissalSource::kPageChanged);
  }

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    // Exit early if the overlay is off or already closing.
    if (lens_overlay_controller_->state() == State::kOff ||
        lens_overlay_controller_->IsOverlayClosing()) {
      return;
    }

    lens_overlay_controller_->CloseUISync(
        status == base::TERMINATION_STATUS_NORMAL_TERMINATION
            ? lens::LensOverlayDismissalSource::kPageRendererClosedNormally
            : lens::LensOverlayDismissalSource::
                  kPageRendererClosedUnexpectedly);
  }

 private:
  raw_ptr<LensOverlayController> lens_overlay_controller_;
};

void LensOverlayController::CaptureScreenshot() {
  state_ = State::kScreenshot;

  // Begin the process of grabbing a screenshot.
  content::RenderWidgetHostView* view = tab_->GetContents()
                                            ->GetPrimaryMainFrame()
                                            ->GetRenderViewHost()
                                            ->GetWidget()
                                            ->GetView();

  // During initialization and shutdown a capture may not be possible.
  if (!IsScreenshotPossible(view)) {
    CloseUISync(
        lens::LensOverlayDismissalSource::kErrorScreenshotCreationFailed);
    return;
  }

  // Side panel is now full closed, take screenshot and open overlay.
  view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(
              &LensOverlayController::FetchViewportImageBoundingBoxes,
              weak_factory_.GetWeakPtr())));
}

void LensOverlayController::FetchViewportImageBoundingBoxes(
    const SkBitmap& bitmap) {
  content::RenderFrameHost* render_frame_host =
      tab_->GetContents()->GetPrimaryMainFrame();
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);
  // Bind the InterfacePtr into the callback so that it's kept alive until
  // there's either a connection error or a response.
  auto* frame = chrome_render_frame.get();

  frame->RequestBoundsHintForAllImages(base::BindOnce(
      &LensOverlayController::GetPdfCurrentPage, weak_factory_.GetWeakPtr(),
      std::move(chrome_render_frame), ++screenshot_attempt_id_, bitmap));
}

void LensOverlayController::GetPdfCurrentPage(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    int attempt_id,
    const SkBitmap& bitmap,
    const std::vector<gfx::Rect>& bounds) {
#if BUILDFLAG(ENABLE_PDF)
  if (lens::features::SendPdfCurrentPageEnabled()) {
    pdf::PDFDocumentHelper* pdf_helper =
        pdf::PDFDocumentHelper::MaybeGetForWebContents(tab_->GetContents());
    if (pdf_helper) {
      pdf_helper->GetMostVisiblePageIndex(base::BindOnce(
          &LensOverlayController::DidCaptureScreenshot,
          weak_factory_.GetWeakPtr(), std::move(chrome_render_frame),
          attempt_id, bitmap, bounds));
      return;
    }
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  DidCaptureScreenshot(std::move(chrome_render_frame), attempt_id, bitmap,
                       bounds, /*pdf_current_page=*/std::nullopt);
}

void LensOverlayController::DidCaptureScreenshot(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    int attempt_id,
    const SkBitmap& bitmap,
    const std::vector<gfx::Rect>& all_bounds,
    std::optional<uint32_t> pdf_current_page) {
  // While capturing a screenshot the overlay was cancelled. Do nothing.
  if (state_ == State::kOff || IsOverlayClosing()) {
    return;
  }

  // An id mismatch implies this is not the most recent screenshot attempt.
  if (screenshot_attempt_id_ != attempt_id) {
    return;
  }

  // The documentation for CopyFromSurface claims that the copy can fail, but
  // without providing information about how this can happen.
  // Supposedly IsSurfaceAvailableForCopy() should guard against this case, but
  // this is a multi-process, multi-threaded environment so there may be a
  // TOCTTOU race condition.
  if (bitmap.drawsNothing()) {
    CloseUISync(
        lens::LensOverlayDismissalSource::kErrorScreenshotCreationFailed);
    return;
  }

  if (lens::features::IsLensOverlayEarlyStartQueryFlowOptimizationEnabled()) {
    // Start the query as soon as the image is ready since it is the only
    // critical asynchronous flow. This optimization parallelizes the query flow
    // with other async startup processes.
    lens_overlay_query_controller_->StartQueryFlow(
        bitmap, GetPageURL(), GetPageTitle(),
        ConvertSignificantRegionBoxes(all_bounds),
        std::vector<lens::PageContent>(), lens::MimeType::kUnknown,
        pdf_current_page, GetUiScaleFactor(), invocation_time_);
  }

  // The following two methods happen async to parallelize the two bottlenecks
  // in our invocation flow.
  CreateInitializationData(bitmap, all_bounds, pdf_current_page);
  ShowOverlay();

  state_ = State::kStartingWebUI;
}

void LensOverlayController::CreateInitializationData(
    const SkBitmap& screenshot,
    const std::vector<gfx::Rect>& all_bounds,
    std::optional<uint32_t> pdf_current_page) {
  // Create the new RGB bitmap async to prevent the main thread from blocking on
  // the encoding.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&CreateRgbBitmap, screenshot),
      base::BindOnce(&LensOverlayController::ContinueCreateInitializationData,
                     weak_factory_.GetWeakPtr(), screenshot, all_bounds,
                     pdf_current_page));
}

void LensOverlayController::ContinueCreateInitializationData(
    const SkBitmap& screenshot,
    const std::vector<gfx::Rect>& all_bounds,
    std::optional<uint32_t> pdf_current_page,
    SkBitmap rgb_screenshot) {
  if (state_ != State::kStartingWebUI || rgb_screenshot.drawsNothing()) {
    // TODO(b/334185985): Handle case when screenshot RGB encoding fails.
    CloseUISync(
        lens::LensOverlayDismissalSource::kErrorScreenshotEncodingFailed);
    return;
  }

  // Resolve the color palette based on the vibrant screenshot color.
  lens::PaletteId color_palette = lens::PaletteId::kFallback;
  if (lens::features::IsDynamicThemeDetectionEnabled()) {
    std::vector<SkColor> colors;
    for (const auto& pair : lens::kPalettes) {
      colors.emplace_back(pair.first);
    }
    SkColor screenshot_color = lens::ExtractVibrantOrDominantColorFromImage(
        screenshot, lens::features::DynamicThemeMinPopulationPct());
    SkColor theme_color = lens::FindBestMatchedColorOrTransparent(
        colors, screenshot_color, lens::features::DynamicThemeMinChroma());
    if (theme_color != SK_ColorTRANSPARENT) {
      color_palette = lens::kPalettes.at(theme_color);
    }
  }

  auto initialization_data = std::make_unique<OverlayInitializationData>(
      screenshot, std::move(rgb_screenshot), color_palette, GetPageURL(),
      GetPageTitle());
  initialization_data->significant_region_boxes_ =
      ConvertSignificantRegionBoxes(all_bounds);
  initialization_data->last_retrieved_most_visible_page_ = pdf_current_page;

  GetPageContextualization(base::BindOnce(
      &LensOverlayController::StorePageContentAndContinueInitialization,
      weak_factory_.GetWeakPtr(), std::move(initialization_data)));
}

void LensOverlayController::StorePageContentAndContinueInitialization(
    std::unique_ptr<OverlayInitializationData> initialization_data,
    std::vector<lens::PageContent> page_contents,
    lens::MimeType primary_content_type,
    std::optional<uint32_t> page_count) {
  initialization_data->page_contents_ = page_contents;
  initialization_data->primary_content_type_ = primary_content_type;
  initialization_data->pdf_page_count_ = page_count;
  InitializeOverlay(std::move(initialization_data));

  RecordDocumentMetrics(page_count);
}

void LensOverlayController::GetPageContextualization(
    PageContentRetrievedCallback callback) {
  // If the contextual searchbox is disabled, exit early.
  if (!lens::features::IsLensOverlayContextualSearchboxEnabled()) {
    std::move(callback).Run(/*page_contents=*/{}, lens::MimeType::kUnknown,
                            std::nullopt);
    return;
  }

#if BUILDFLAG(ENABLE_PDF)
  // Try and fetch the PDF bytes if enabled.
  pdf::PDFDocumentHelper* pdf_helper =
      lens::features::UsePdfsAsContext()
          ? pdf::PDFDocumentHelper::MaybeGetForWebContents(tab_->GetContents())
          : nullptr;
  if (pdf_helper) {
    // Fetch the PDF bytes then initialize the overlay.
    pdf_helper->GetPdfBytes(
        /*size_limit=*/lens::features::GetLensOverlayFileUploadLimitBytes(),
        base::BindOnce(&LensOverlayController::OnPdfBytesReceived,
                       weak_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  std::vector<lens::PageContent> page_contents;
  auto* render_frame_host = tab_->GetContents()->GetPrimaryMainFrame();
  if (!render_frame_host || (!lens::features::UseInnerHtmlAsContext() &&
                             !lens::features::UseInnerTextAsContext() &&
                             !lens::features::UseApcAsContext())) {
    std::move(callback).Run(page_contents, lens::MimeType::kUnknown,
                            std::nullopt);
    return;
  }
  // TODO(crbug.com/399610478): The fetches for innerHTML, innerText, and APC
  // should be parallelized to fetch all data at once. Currently fetches are
  // sequential to prevent getting stuck in a race condition.
  MaybeGetInnerHtml(page_contents, render_frame_host, std::move(callback));
}

#if BUILDFLAG(ENABLE_PDF)
void LensOverlayController::OnPdfBytesReceived(
    PageContentRetrievedCallback callback,
    pdf::mojom::PdfListener::GetPdfBytesStatus status,
    const std::vector<uint8_t>& bytes,
    uint32_t page_count) {
  // TODO(b/370530197): Show user error message if status is not success.
  if (status != pdf::mojom::PdfListener::GetPdfBytesStatus::kSuccess ||
      page_count == 0) {
    std::move(callback).Run(
        {lens::PageContent(/*bytes=*/{}, lens::MimeType::kPdf)},
        lens::MimeType::kPdf, page_count);
    return;
  }
  std::move(callback).Run({lens::PageContent(bytes, lens::MimeType::kPdf)},
                          lens::MimeType::kPdf, page_count);
}

void LensOverlayController::FetchVisiblePageIndexAndGetPartialPdfText(
    uint32_t page_count) {
  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(tab_->GetContents());
  if (!pdf_helper ||
      lens::features::GetLensOverlayPdfSuggestCharacterTarget() == 0 ||
      page_count == 0) {
    return;
  }

  // TODO(387306854): Add logic to grab page text form the visible page index.

  // Fetch the first page of text which will be then recursively fetch following
  // pages.
  initialization_data_->pdf_pages_text_.clear();
  pdf_helper->GetPageText(
      /*page_index=*/0,
      base::BindOnce(&LensOverlayController::GetPartialPdfTextCallback,
                     weak_factory_.GetWeakPtr(), /*page_index=*/0, page_count,
                     /*total_characters_retrieved=*/0));
}

void LensOverlayController::GetPartialPdfTextCallback(
    uint32_t page_index,
    uint32_t total_page_count,
    uint32_t total_characters_retrieved,
    const std::u16string& page_text) {
  // Sanity checks that the input is expected.
  CHECK_GE(total_page_count, 1u);
  CHECK_LT(page_index, total_page_count);
  CHECK_EQ(initialization_data_->pdf_pages_text_.size(), page_index);

  // Add the page text to the list of pages and update the total characters
  // retrieved count.
  initialization_data_->pdf_pages_text_.push_back(page_text);

  // Ensure no integer overflow. If overflow, set the total characters retrieved
  // to the max value so the loop will exit.
  base::CheckedNumeric<uint32_t> total_characters_retrieved_check =
      total_characters_retrieved;
  total_characters_retrieved_check += page_text.size();
  total_characters_retrieved = total_characters_retrieved_check.ValueOrDefault(
      std::numeric_limits<uint32_t>::max());

  pdf::PDFDocumentHelper* pdf_helper =
      pdf::PDFDocumentHelper::MaybeGetForWebContents(tab_->GetContents());

  // Stop the loop if the character limit is reached or if the page index is
  // out of bounds or the PDF helper no longer exists.
  if (!pdf_helper ||
      total_characters_retrieved >=
          lens::features::GetLensOverlayPdfSuggestCharacterTarget() ||
      page_index + 1 >= total_page_count) {
    lens_overlay_query_controller_->SendPartialPageContentRequest(
        initialization_data_->pdf_pages_text_);
    return;
  }

  pdf_helper->GetPageText(
      page_index + 1,
      base::BindOnce(&LensOverlayController::GetPartialPdfTextCallback,
                     weak_factory_.GetWeakPtr(), page_index + 1,
                     total_page_count, total_characters_retrieved));
}
#endif  // BUILDFLAG(ENABLE_PDF)

void LensOverlayController::MaybeGetInnerHtml(
    std::vector<lens::PageContent> page_contents,
    content::RenderFrameHost* render_frame_host,
    PageContentRetrievedCallback callback) {
  if (!lens::features::UseInnerHtmlAsContext()) {
    MaybeGetInnerText(page_contents, render_frame_host, std::move(callback));
    return;
  }
  content_extraction::GetInnerHtml(
      *render_frame_host,
      base::BindOnce(&LensOverlayController::OnInnerHtmlReceived,
                     weak_factory_.GetWeakPtr(), page_contents,
                     render_frame_host, std::move(callback)));
}

void LensOverlayController::OnInnerHtmlReceived(
    std::vector<lens::PageContent> page_contents,
    content::RenderFrameHost* render_frame_host,
    PageContentRetrievedCallback callback,
    const std::optional<std::string>& result) {
  const bool was_successful =
      result.has_value() &&
      result->size() <= lens::features::GetLensOverlayFileUploadLimitBytes();
  // Add the innerHTML to the page contents if successful, or empty bytes if
  // not.
  page_contents.emplace_back(
      /*bytes=*/was_successful
          ? std::vector<uint8_t>(result->begin(), result->end())
          : std::vector<uint8_t>{},
      lens::MimeType::kHtml);
  MaybeGetInnerText(page_contents, render_frame_host, std::move(callback));
}

void LensOverlayController::MaybeGetInnerText(
    std::vector<lens::PageContent> page_contents,
    content::RenderFrameHost* render_frame_host,
    PageContentRetrievedCallback callback) {
  if (!lens::features::UseInnerTextAsContext()) {
    MaybeGetAnnotatedPageContent(page_contents, render_frame_host,
                                 std::move(callback));
    return;
  }
  content_extraction::GetInnerText(
      *render_frame_host, /*node_id=*/std::nullopt,
      base::BindOnce(&LensOverlayController::OnInnerTextReceived,
                     weak_factory_.GetWeakPtr(), page_contents,
                     render_frame_host, std::move(callback)));
}

void LensOverlayController::OnInnerTextReceived(
    std::vector<lens::PageContent> page_contents,
    content::RenderFrameHost* render_frame_host,
    PageContentRetrievedCallback callback,
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  const bool was_successful =
      result && result->inner_text.size() <=
                    lens::features::GetLensOverlayFileUploadLimitBytes();
  // Add the innerText to the page_contents if successful, or empty bytes if
  // not.
  page_contents.emplace_back(
      /*bytes=*/was_successful
          ? std::vector<uint8_t>(result->inner_text.begin(),
                                 result->inner_text.end())
          : std::vector<uint8_t>{},
      lens::MimeType::kPlainText);
  MaybeGetAnnotatedPageContent(page_contents, render_frame_host,
                               std::move(callback));
}

void LensOverlayController::MaybeGetAnnotatedPageContent(
    std::vector<lens::PageContent> page_contents,
    content::RenderFrameHost* render_frame_host,
    PageContentRetrievedCallback callback) {
  if (!lens::features::UseApcAsContext()) {
    // Done fetching page contents.
    // Keep legacy behavior consistent by setting the primary content type to
    // plain text if that is the only content type enabled.
    // TODO(crbug.com/401614601): Set primary content type to kHtml in all
    // cases.
    auto primary_content_type = lens::features::UseInnerTextAsContext() &&
                                        !lens::features::UseInnerHtmlAsContext()
                                    ? lens::MimeType::kPlainText
                                    : lens::MimeType::kHtml;
    std::move(callback).Run(page_contents, primary_content_type, std::nullopt);
    return;
  }

  blink::mojom::AIPageContentOptionsPtr ai_page_content_options =
      optimization_guide::DefaultAIPageContentOptions();
  ai_page_content_options->on_critical_path = true;
  optimization_guide::GetAIPageContent(
      tab_->GetContents(), std::move(ai_page_content_options),
      base::BindOnce(&LensOverlayController::OnAnnotatedPageContentReceived,
                     weak_factory_.GetWeakPtr(), page_contents,
                     std::move(callback)));
}

void LensOverlayController::OnAnnotatedPageContentReceived(
    std::vector<lens::PageContent> page_contents,
    PageContentRetrievedCallback callback,
    std::optional<optimization_guide::AIPageContentResult> result) {
  // Add the apc proto the page_contents if it exists.
  if (result) {
    std::string serialized_apc;
    result->proto.SerializeToString(&serialized_apc);
    page_contents.emplace_back(
        std::vector<uint8_t>(serialized_apc.begin(), serialized_apc.end()),
        lens::MimeType::kAnnotatedPageContent);
  }
  // Done fetching page contents.
  std::move(callback).Run(page_contents, lens::MimeType::kAnnotatedPageContent,
                          std::nullopt);
}

std::vector<lens::mojom::CenterRotatedBoxPtr>
LensOverlayController::ConvertSignificantRegionBoxes(
    const std::vector<gfx::Rect>& all_bounds) {
  std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes;
  int max_regions = lens::features::GetLensOverlayMaxSignificantRegions();
  if (max_regions == 0) {
    return significant_region_boxes;
  }
  content::RenderFrameHost* render_frame_host =
      tab_->GetContents()->GetPrimaryMainFrame();
  auto view_bounds = render_frame_host->GetView()->GetViewBounds();
  for (auto& image_bounds : all_bounds) {
    // Check the original area of the images against the minimum area.
    if (image_bounds.width() * image_bounds.height() >=
        lens::features::GetLensOverlaySignificantRegionMinArea()) {
      // We only have bounds for images in the main frame of the tab (i.e. not
      // in iframes), so view bounds are identical to tab bounds and can be
      // used for both parameters.
      significant_region_boxes.emplace_back(
          lens::GetCenterRotatedBoxFromTabViewAndImageBounds(
              view_bounds, view_bounds, image_bounds));
    }
  }
  // If an image is outside the viewpoint, the box will have zero area.
  std::erase_if(significant_region_boxes, [](const auto& box) {
    return box->box.height() == 0 || box->box.width() == 0;
  });
  // Sort by descending area.
  std::sort(significant_region_boxes.begin(), significant_region_boxes.end(),
            [](const auto& box1, const auto& box2) {
              return box1->box.height() * box1->box.width() >
                     box2->box.height() * box2->box.width();
            });
  // Treat negative values of max_regions as no limit.
  if (max_regions > 0 &&
      significant_region_boxes.size() > (unsigned long)max_regions) {
    significant_region_boxes.resize(max_regions);
  }

  return significant_region_boxes;
}

void LensOverlayController::TryUpdatePageContextualization() {
  // If there is already an upload, do not send another request.
  // TODO(crbug.com/399154548): Ideally, there could be two uploads in progress
  // at a time, however, the current query controller implementation does not
  // support this.
  if (lens_overlay_query_controller_->IsPageContentUploadInProgress()) {
    return;
  }

  GetPageContextualization(
      base::BindOnce(&LensOverlayController::UpdatePageContextualization,
                     weak_factory_.GetWeakPtr()));
}

void LensOverlayController::UpdatePageContextualization(
    std::vector<lens::PageContent> page_contents,
    lens::MimeType primary_content_type,
    std::optional<uint32_t> page_count) {
  if (!lens::features::IsLensOverlayContextualSearchboxEnabled()) {
    return;
  }

  // Do not capture a new screenshot if the feature param is not enabled or if
  // the user is not viewing the live page, meaning the viewport cannot have
  // changed.
  if (!lens::features::UpdateViewportEachQueryEnabled() ||
      state_ != State::kLivePageAndResults) {
    UpdatePageContextualizationPart2(page_contents, primary_content_type,
                                     page_count, SkBitmap());
    return;
  }

  // Begin the process of grabbing a screenshot.
  content::RenderWidgetHostView* view = tab_->GetContents()
                                            ->GetPrimaryMainFrame()
                                            ->GetRenderViewHost()
                                            ->GetWidget()
                                            ->GetView();
  if (!IsScreenshotPossible(view)) {
    UpdatePageContextualizationPart2(page_contents, primary_content_type,
                                     page_count, SkBitmap());
    return;
  }
  view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(
              &LensOverlayController::UpdatePageContextualizationPart2,
              weak_factory_.GetWeakPtr(), page_contents, primary_content_type,
              page_count)));
}

void LensOverlayController::UpdatePageContextualizationPart2(
    std::vector<lens::PageContent> page_contents,
    lens::MimeType primary_content_type,
    std::optional<uint32_t> page_count,
    const SkBitmap& bitmap) {
#if BUILDFLAG(ENABLE_PDF)
  if (lens::features::SendPdfCurrentPageEnabled()) {
    pdf::PDFDocumentHelper* pdf_helper =
        pdf::PDFDocumentHelper::MaybeGetForWebContents(tab_->GetContents());
    if (pdf_helper) {
      pdf_helper->GetMostVisiblePageIndex(base::BindOnce(
          &LensOverlayController::UpdatePageContextualizationPart3,
          weak_factory_.GetWeakPtr(), page_contents, primary_content_type,
          page_count, bitmap));
      return;
    }
  }
#endif  // BUILDFLAG(ENABLE_PDF)

  UpdatePageContextualizationPart3(page_contents, primary_content_type,
                                   page_count, bitmap,
                                   /*most_visible_page=*/std::nullopt);
}

void LensOverlayController::UpdatePageContextualizationPart3(
    std::vector<lens::PageContent> page_contents,
    lens::MimeType primary_content_type,
    std::optional<uint32_t> page_count,
    const SkBitmap& bitmap,
    std::optional<uint32_t> most_visible_page) {
  bool sending_bitmap = false;
  if (!bitmap.drawsNothing() &&
      (initialization_data_->updated_screenshot_.drawsNothing() ||
       !lens::AreBitmapsEqual(initialization_data_->updated_screenshot_,
                              bitmap))) {
    initialization_data_->updated_screenshot_ = bitmap;
    sending_bitmap = true;
  }
  initialization_data_->last_retrieved_most_visible_page_ = most_visible_page;

  // TODO(crbug.com/399215935): Ideally, this check should ensure that any of
  // the content date has not changed. For now, we only check if the
  // primary_content_type bytes have changed.
  auto old_page_content_it = std::ranges::find_if(
      initialization_data_->page_contents_,
      [&primary_content_type](const auto& page_content) {
        return page_content.content_type_ == primary_content_type;
      });
  auto new_page_content_it = std::ranges::find_if(
      page_contents, [&primary_content_type](const auto& page_content) {
        return page_content.content_type_ == primary_content_type;
      });
  const lens::PageContent* old_page_content =
      old_page_content_it != initialization_data_->page_contents_.end()
          ? &(*old_page_content_it)
          : nullptr;
  const lens::PageContent* new_page_content =
      new_page_content_it != page_contents.end() ? &(*new_page_content_it)
                                                 : nullptr;

  if (initialization_data_->primary_content_type_ == primary_content_type &&
      old_page_content && new_page_content) {
    const float old_size = old_page_content->bytes_.size();
    const float new_size = new_page_content->bytes_.size();
    const float percent_changed = abs((new_size - old_size) / old_size);
    if (percent_changed < kByteChangeTolerancePercent) {
      if (!sending_bitmap) {
        // If the bytes have not changed more than our threshold and the
        // screenshot has not changed, exit early. Notify the query controller
        // that the user may be issuing a search request, and therefore the
        // query should be restarted if TTL expired. If the bytes did change,
        // this will happen automatically as a result of the
        // SendUpdatedPageContent call below.
        lens_overlay_query_controller_->MaybeRestartQueryFlow();
        return;
      }
      // If the screenshot has changed but the bytes have not, send only the
      // screenshot.
      lens_overlay_query_controller_->SendUpdatedPageContent(
          std::nullopt, std::nullopt, std::nullopt, std::nullopt,
          initialization_data_->last_retrieved_most_visible_page_,
          sending_bitmap ? bitmap : SkBitmap());
      return;
    }
  }

  // Since the page content has changed, let the query controller know to avoid
  // dangling pointers.
  lens_overlay_query_controller_->ResetPageContentData();

  initialization_data_->page_contents_ = page_contents;
  initialization_data_->primary_content_type_ = primary_content_type;

  // If no bytes were retrieved from the page, the query won't be able to be
  // contexualized. Notify the side panel so the ghost loader isn't shown. No
  // need to update update the overlay as this update only happens on navigation
  // where the side panel will already be open.
  if (!new_page_content || new_page_content->bytes_.empty()) {
    SuppressGhostLoader();
  }

#if BUILDFLAG(ENABLE_PDF)
  // If the new page is a PDF, fetch the text from the page to be used as early
  // suggest signals.
  if (new_page_content &&
      new_page_content->content_type_ == lens::MimeType::kPdf) {
    FetchVisiblePageIndexAndGetPartialPdfText(page_count.value_or(0));
  }
#endif

  is_upload_progress_bar_shown_ = true;
  is_first_upload_handler_event_ = true;
  lens_overlay_query_controller_->SendUpdatedPageContent(
      initialization_data_->page_contents_,
      initialization_data_->primary_content_type_, GetPageURL(), GetPageTitle(),
      initialization_data_->last_retrieved_most_visible_page_,
      sending_bitmap ? bitmap : SkBitmap());

  RecordDocumentMetrics(page_count);
}

void LensOverlayController::SuppressGhostLoader() {
  if (page_) {
    page_->SuppressGhostLoader();
  }
  results_side_panel_coordinator_->SuppressGhostLoader();
}

void LensOverlayController::SetLiveBlur(bool enabled) {
  if (!lens_overlay_blur_layer_delegate_) {
    return;
  }

  if (enabled) {
    lens_overlay_blur_layer_delegate_->StartBackgroundImageCapture();
    return;
  }

  lens_overlay_blur_layer_delegate_->StopBackgroundImageCapture();
}

void LensOverlayController::ShowOverlay() {
  // Grab the tab contents web view and disable mouse and keyboard inputs to it.
  auto* contents_web_view = tab_->GetBrowserWindowInterface()->GetWebView();
  CHECK(contents_web_view);
  if (should_show_overlay_) {
    contents_web_view->SetEnabled(false);
  }

  // If the view already exists, we just need to reshow it.
  if (overlay_view_) {
    // Exit early to avoid reshowing the overlay if it should not be shown.
    if (!should_show_overlay_) {
      return;
    }
    // Restore the state to show the overlay.
    overlay_view_->SetVisible(true);
    preselection_widget_anchor_->SetVisible(true);
    overlay_web_view_->SetVisible(true);

    // Restart the live blur since the view is visible again.
    SetLiveBlur(true);

    // The overlay needs to be focused on show to immediately begin
    // receiving key events.
    overlay_web_view_->RequestFocus();
    return;
  }

  // Create the views that will house our UI. The overlay view might not
  // actually be shown, as dictated by `should_show_overlay_`. It still needs to
  // be created so the initialization process completes.
  overlay_view_ = CreateViewForOverlay();
  overlay_view_->SetVisible(should_show_overlay_);

  // Sanity check that the overlay view is above the contents web view.
  auto* parent_view = overlay_view_->parent();
  views::View* child_contents_view = contents_web_view;
  // TODO(crbug.com/406794005): Remove this block if overlay_view_ ends up
  // getting reparented such that it always shares a parent with
  // contents_web_view.
  if (base::FeatureList::IsEnabled(features::kSideBySide)) {
    // When split view is enabled, there are two additional layers of
    // hierarchy:
    // BrowserView->MultiContentsView->ContentsContainerView->ContentsWebView
    // vs.
    // BrowserView->ContentsWebView
    // Since the overlay view is parented by BrowserView, to properly pass the
    // check below, we should only compare direct children of BrowserView.
    child_contents_view = child_contents_view->parent()->parent();
  }
  CHECK(parent_view->GetIndexOf(overlay_view_) >
        parent_view->GetIndexOf(child_contents_view));

  // Observe the overlay view to handle resizing the background blur layer.
  tab_contents_view_observer_.Observe(overlay_view_);

  // The overlay needs to be focused on show to immediately begin
  // receiving key events.
  if (should_show_overlay_) {
    CHECK(overlay_web_view_);
    overlay_web_view_->RequestFocus();
  }

  // Listen to the render process housing out overlay.
  overlay_web_view_->GetWebContents()
      ->GetPrimaryMainFrame()
      ->GetProcess()
      ->AddObserver(this);
}

void LensOverlayController::HideOverlay() {
  // Hide the overlay view, but keep the web view attached to the overlay view
  // so that the overlay can be re-shown without creating a new web view.
  preselection_widget_anchor_->SetVisible(false);
  overlay_web_view_->SetVisible(false);
  MaybeHideSharedOverlayView();

  SetLiveBlur(false);
  HidePreselectionBubble();
  // Re-enable mouse and keyboard events to the tab contents web view.
  auto* contents_web_view = tab_->GetBrowserWindowInterface()->GetWebView();
  CHECK(contents_web_view);
  contents_web_view->SetEnabled(true);
}

void LensOverlayController::MaybeHideSharedOverlayView() {
  if (!overlay_view_) {
    return;
  }
  for (views::View* child : overlay_view_->children()) {
    if (child->GetVisible()) {
      // If any child is visible, it is being used by another tab so do not hide
      // the overlay view.
      return;
    }
  }
  overlay_view_->SetVisible(false);
}

void LensOverlayController::MaybeOpenSidePanel() {
  if (side_panel_in_use_) {
    // Exit early if this class has already requested access to the side panel.
    return;
  }
  side_panel_in_use_ = results_side_panel_coordinator_->RegisterEntryAndShow();
}

void LensOverlayController::CloseUIPart2(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (state_ == State::kOff) {
    return;
  }

  // Ensure that this path is not being used to close the overlay if the overlay
  // is currently showing. If the overlay is currently showing, CloseUIAsync
  // should be used instead.
  CHECK(state_ != State::kOverlay);
  CHECK(state_ != State::kOverlayAndResults);

  // TODO(b/331940245): Refactor to be decoupled from permission_prompt_factory
  state_ = State::kClosing;

  // Closes preselection toast if it exists.
  ClosePreselectionBubble();

  // A permission prompt may be suspended if the overlay was showing when the
  // permission was queued. Restore the suspended prompt if possible.
  // TODO(b/331940245): Refactor to be decoupled from PermissionPromptFactory
  content::WebContents* contents = tab_->GetContents();
  CHECK(contents);
  auto* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(contents);
  if (permission_request_manager &&
      permission_request_manager->CanRestorePrompt()) {
    permission_request_manager->RestorePrompt();
  }

  permission_bubble_controller_.reset();
  side_panel_searchbox_handler_.reset();
  results_side_panel_coordinator_ = nullptr;
  side_panel_in_use_.reset();
  pre_initialization_suggest_inputs_.reset();
  pre_initialization_objects_.reset();
  pre_initialization_text_.reset();

  side_panel_shown_subscription_ = base::CallbackListSubscription();
  side_panel_coordinator_ = nullptr;

  // Re-enable mouse and keyboard events to the tab contents web view.
  auto* contents_web_view = tab_->GetBrowserWindowInterface()->GetWebView();
  CHECK(contents_web_view);
  contents_web_view->SetEnabled(true);

  if (overlay_web_view_) {
    // Remove render frame observer.
    overlay_web_view_->GetWebContents()
        ->GetPrimaryMainFrame()
        ->GetProcess()
        ->RemoveObserver(this);
  }

  // LensOverlayQueryController points to initialization data and therefore must
  // be reset before the initialization data to avoid dangling pointers.
  lens_overlay_query_controller_.reset();
  initialization_data_.reset();

  tab_contents_view_observer_.Reset();
  omnibox_tab_helper_observer_.Reset();
  find_tab_observer_.Reset();
  receiver_.reset();
  page_.reset();
  languages_controller_.reset();
  scoped_tab_modal_ui_.reset();
  pending_text_query_.reset();
  pending_thumbnail_uri_.reset();
  selected_region_thumbnail_uri_.clear();
  pending_region_.reset();
  fullscreen_observation_.Reset();
  immersive_mode_observer_.Reset();
  lens_overlay_blur_layer_delegate_.reset();
  overlay_searchbox_handler_.reset();
  last_navigation_time_.reset();
#if BUILDFLAG(IS_MAC)
  pref_change_registrar_.Reset();
#endif  // BUILDFLAG(IS_MAC)

  // Cleanup all of the lens overlay related views. The overlay view is owned by
  // the browser view and is reused for each Lens overlay session. Clean it up
  // so it is ready for the next invocation.
  if (overlay_view_) {
    overlay_view_->RemoveChildViewT(
        std::exchange(preselection_widget_anchor_, nullptr));
    overlay_view_->RemoveChildViewT(std::exchange(overlay_web_view_, nullptr));
    MaybeHideSharedOverlayView();
    overlay_view_ = nullptr;
  }

  lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
  should_show_overlay_ = true;

  state_ = State::kOff;

  // Update the entrypoints now that the controller is closed.
  UpdateEntryPointsState();

  RecordEndOfSessionMetrics(dismissal_source);
}

void LensOverlayController::InitializeOverlay(
    std::unique_ptr<OverlayInitializationData> initialization_data) {
  // Initialization data is ready.
  if (initialization_data) {
    // Confirm initialization_data has not already been assigned.
    CHECK(!initialization_data_);
    initialization_data_ = std::move(initialization_data);

    // If suggest inputs were updated before the initialization data was ready,
    // attach them to the initialization data now.
    if (pre_initialization_suggest_inputs_.has_value()) {
      initialization_data_->suggest_inputs_ =
          pre_initialization_suggest_inputs_.value();
      pre_initialization_suggest_inputs_.reset();
    }
  }

  // We can only continue once both the WebUI is bound and the initialization
  // data is processed and ready. If either of those conditions aren't met, we
  // exit early and wait for the other condition to call this method again.
  if (!page_ || !initialization_data_) {
    return;
  }

  // Move the data that was stored prior to initialization into
  // initialization_data_.
  if (pre_initialization_objects_.has_value()) {
    initialization_data_->objects_ =
        std::move(pre_initialization_objects_.value());
    pre_initialization_objects_.reset();
  }
  if (pre_initialization_text_.has_value()) {
    initialization_data_->text_ = std::move(pre_initialization_text_.value());
    pre_initialization_text_.reset();
  }

  InitializeOverlayUI(*initialization_data_);
  base::UmaHistogramBoolean("Lens.Overlay.Shown", true);

#if BUILDFLAG(ENABLE_PDF)
  // If PDF content was extracted from the page, fetch the text from the PDF to
  // be used as early suggest signals.
  if (!initialization_data_->page_contents_.empty() &&
      initialization_data_->page_contents_.front().content_type_ ==
          lens::MimeType::kPdf) {
    CHECK(initialization_data_->pdf_page_count_.has_value());
    FetchVisiblePageIndexAndGetPartialPdfText(
        initialization_data_->pdf_page_count_.value());
  }
#endif

  // If the StartQueryFlow optimization is enabled, the page contents will not
  // be sent with the initial image request, so we need to send it here.
  if (lens::features::IsLensOverlayContextualSearchboxEnabled() &&
      lens::features::IsLensOverlayEarlyStartQueryFlowOptimizationEnabled()) {
    lens_overlay_query_controller_->SendUpdatedPageContent(
        initialization_data_->page_contents_,
        initialization_data_->primary_content_type_, GetPageURL(),
        GetPageTitle(), initialization_data_->last_retrieved_most_visible_page_,
        SkBitmap());
  }

  // Show the preselection overlay now that the overlay is initialized and ready
  // to be shown.
  if (!pending_region_ && should_show_overlay_) {
    ShowPreselectionBubble();
  }

  // Create the blur delegate so it is ready to blur once the view is visible.
  if (lens::features::GetLensOverlayUseBlur()) {
    content::RenderWidgetHost* live_page_widget_host =
        tab_->GetContents()
            ->GetPrimaryMainFrame()
            ->GetRenderViewHost()
            ->GetWidget();
    lens_overlay_blur_layer_delegate_ =
        std::make_unique<lens::LensOverlayBlurLayerDelegate>(
            live_page_widget_host);
  }

  state_ = State::kOverlay;

  // Update the entry points state to ensure that the entry points are disabled
  // now that the overlay is showing.
  UpdateEntryPointsState();

  // Only start the query flow again if we don't already have a full image
  // response, unless the early start query flow optimization is enabled.
  if (!initialization_data_->has_full_image_response() &&
      !lens::features::IsLensOverlayEarlyStartQueryFlowOptimizationEnabled()) {
    // Use std::move because significant_region_boxes_ is only used in this
    // call, which should only occur once in the lifetime of
    // LensOverlayQueryController and thus of LensOverlayController.
    lens_overlay_query_controller_->StartQueryFlow(
        initialization_data_->initial_screenshot_,
        initialization_data_->page_url_, initialization_data_->page_title_,
        std::move(initialization_data_->significant_region_boxes_),
        initialization_data_->page_contents_,
        initialization_data_->primary_content_type_,
        initialization_data_->last_retrieved_most_visible_page_,
        GetUiScaleFactor(), invocation_time_);
  }

  // If there is a pending contextual search request, issue it now that the
  // overlay is initialized.
  if (pending_contextual_search_request_) {
    std::move(pending_contextual_search_request_).Run();
  }

  // TODO(b/352622136): We should not start the lens request until the overlay
  // is open to prevent the side panel from opening while the overlay UI is
  // rendering.
  if (pending_region_) {
    // If there is a pending region (i.e. for image right click)
    // use INJECTED_IMAGE as the selection type.
    IssueLensRequest(std::move(pending_region_), lens::INJECTED_IMAGE,
                     pending_region_bitmap_);
    pending_region_bitmap_.reset();
  }
}

void LensOverlayController::InitializeOverlayUI(
    const OverlayInitializationData& init_data) {
  // This should only contain LensPage mojo calls and should not affect
  // `state_`.
  CHECK(page_);
  // TODO(b/371593619), it would be more efficent to send all initialization
  // data to the overlay web UI in a single message.
  page_->ThemeReceived(CreateTheme(init_data.color_palette_));

  bool should_show_csb = !init_data.page_contents_.empty() &&
                         !init_data.page_contents_.front().bytes_.empty();
  if (should_show_csb) {
    // Reset metric booleans in case they were set to true previously and the
    // overlay was reopened.
    csb_session_end_metrics_ = lens::ContextualSearchboxSessionEndMetrics();
    csb_session_end_metrics_.searchbox_shown_ = true;
  }
  initial_page_content_type_ =
      init_data.page_contents_.empty()
          ? lens::MimeType::kUnknown
          : init_data.page_contents_.front().content_type_;
  initial_document_type_ = lens::StringMimeTypeToDocumentType(
      tab_->GetContents()->GetContentsMimeType());
  page_->ShouldShowContextualSearchBox(should_show_csb);

  // Send the initial document type to the overlay web UI.
  NotifyPageContentUpdated();

  page_->ScreenshotDataReceived(init_data.initial_rgb_screenshot_);
  if (!init_data.objects_.empty()) {
    SendObjects(CopyObjects(init_data.objects_));
  }
  if (init_data.text_) {
    SendText(init_data.text_->Clone());
  }
  if (pending_region_) {
    page_->SetPostRegionSelection(pending_region_->Clone());
  }
  if (IsHandshakeComplete()) {
    // Notify the overlay that it is safe to query autocomplete.
    page_->NotifyHandshakeComplete();
  }

  // Record the UMA for lens overlay invocation.
  lens::RecordInvocation(invocation_source_, initial_document_type_);
}

raw_ptr<views::View> LensOverlayController::CreateViewForOverlay() {
  // Grab the host view for the overlay which is owned by the browser view.
  auto* host_view = tab_->GetBrowserWindowInterface()->LensOverlayView();
  CHECK(host_view);

  // Setup a preselection anchor view. Usually bubbles are anchored to top
  // chrome, but top chrome is not always visible when our overlay is visible.
  // Instead of anchroing to top chrome, we anchor to this view because 1) it
  // always exists when the overlay exists and 2) it is before the WebView in
  // the view hierarchy and therefore will receive focus first when tabbing from
  // top chrome.
  std::unique_ptr<views::View> anchor_view = std::make_unique<views::View>();
  anchor_view->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  preselection_widget_anchor_ = host_view->AddChildView(std::move(anchor_view));

  // Create the web view.
  std::unique_ptr<views::WebView> web_view = std::make_unique<views::WebView>(
      tab_->GetContents()->GetBrowserContext());
  content::WebContents* web_view_contents = web_view->GetWebContents();
  web_view->SetProperty(views::kElementIdentifierKey, kOverlayId);
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_view_contents, SK_ColorTRANSPARENT);

  // Set the label for the renderer process in Chrome Task Manager.
  task_manager::WebContentsTags::CreateForToolContents(
      web_view_contents, IDS_LENS_OVERLAY_RENDERER_LABEL);

  // As the embedder for the lens overlay WebUI content we must set the
  // appropriate tab interface here.
  webui::SetTabInterface(web_view_contents, GetTabInterface());

  // Set the web contents delegate to this controller so we can handle keyboard
  // events. Allow accelerators (e.g. hotkeys) to work on this web view.
  web_view->set_allow_accelerators(true);
  web_view->GetWebContents()->SetDelegate(this);

  // Load the untrusted WebUI into the web view.
  web_view->LoadInitialURL(GURL(chrome::kChromeUILensOverlayUntrustedURL));

  overlay_web_view_ = host_view->AddChildView(std::move(web_view));
  return host_view;
}

bool LensOverlayController::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // We do not want to show the browser context menu on the overlay unless we
  // are in debugging mode. Returning true is equivalent to not showing the
  // context menu.
  return !lens::features::IsLensOverlayDebuggingEnabled();
}

bool LensOverlayController::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // This can be called before the overlay web view is attached to the overlay
  // view. In that case, the focus manager could be null.
  if (!overlay_web_view_ || !overlay_web_view_->GetFocusManager()) {
    return false;
  }
  return lens_overlay_event_handler_->HandleKeyboardEvent(
      source, event, overlay_web_view_->GetFocusManager());
}

void LensOverlayController::OnFullscreenStateChanged() {
  // Flag is enabled to allow Lens Overlay in fullscreen no matter what so we
  // can exit early.
  if (lens::features::GetLensOverlayEnableInFullscreen()) {
    return;
  }
  // If there is top chrome we can keep the overlay open.
  if (tab_->GetBrowserWindowInterface()->IsTabStripVisible()) {
    return;
  }
  CloseUISync(lens::LensOverlayDismissalSource::kFullscreened);
}

void LensOverlayController::OnViewBoundsChanged(views::View* observed_view) {
  CHECK(observed_view == overlay_view_);

  // We now want to start the live blur since the screenshot has resized to
  // allow the blur to peek through.
  if (IsOverlayShowing()) {
    SetLiveBlur(true);
  }

  // Set our view to the same bounds as the contents web view so it always
  // covers the tab contents.
  if (lens_overlay_blur_layer_delegate_) {
    // Set the blur to have the same bounds as our view, but since it is in our
    // views local coordinate system, the blur should be positioned at (0,0).
    lens_overlay_blur_layer_delegate_->layer()->SetBounds(
        overlay_view_->GetLocalBounds());
  }
}

#if BUILDFLAG(IS_MAC)
void LensOverlayController::OnWidgetActivationChanged(views::Widget* widget,
                                                      bool active) {
  if (active && preselection_widget_) {
    // On Mac, traversing out of the preselection widget into the browser causes
    // the browser to restore its focus to the wrong place. Thus, when entering
    // the preselection widget, make sure to clear out the browser's native
    // focus. This causes the preselection widget to lose activation, so
    // reactivate it manually.
    tab_->GetBrowserWindowInterface()
        ->TopContainer()
        ->GetWidget()
        ->GetFocusManager()
        ->ClearNativeFocus();
    preselection_widget_->Activate();
  }
}
#endif

void LensOverlayController::OnWidgetDestroying(views::Widget* widget) {
  preselection_widget_ = nullptr;
  preselection_widget_observer_.Reset();
}

void LensOverlayController::OnOmniboxFocusChanged(
    OmniboxFocusState state,
    OmniboxFocusChangeReason reason) {
  if (state_ == LensOverlayController::State::kOverlay) {
    if (state == OMNIBOX_FOCUS_NONE) {
      ShowPreselectionBubble();
    } else {
      HidePreselectionBubble();
    }
  }
}

void LensOverlayController::OnFindEmptyText(
    content::WebContents* web_contents) {
  if (state_ == State::kLivePageAndResults) {
    return;
  }
  CloseUIAsync(lens::LensOverlayDismissalSource::kFindInPageInvoked);
}

void LensOverlayController::OnFindResultAvailable(
    content::WebContents* web_contents) {
  if (state_ == State::kLivePageAndResults) {
    return;
  }
  CloseUIAsync(lens::LensOverlayDismissalSource::kFindInPageInvoked);
}

void LensOverlayController::OnImmersiveRevealStarted() {
  // The toolbar has began to reveal. If the overlay is showing, hide the
  // preselection bubble to ensure it doesn't cover with the toolbar UI.
  if (IsOverlayShowing()) {
    HidePreselectionBubble();
  }
}

void LensOverlayController::OnImmersiveRevealEnded() {
  // The toolbar is no longer revealed. If the overlay is showing, reshow the
  // preselection bubble to ensure it doesn't cover with the toolbar UI.
  if (IsOverlayShowing()) {
    ShowPreselectionBubble();
  }
}

void LensOverlayController::OnImmersiveFullscreenEntered() {
  // The browser entered immersive fullscreen. If the overlay is showing, call
  // close and reopen the preselection bubble to ensure it respositions
  // correctly.
  if (IsOverlayShowing()) {
    CloseAndReshowPreselectionBubble();
  }
}

void LensOverlayController::OnImmersiveFullscreenExited() {
  // The browser exited immersive fullscreen. If the overlay is showing, call
  // close and reopen the preselection bubble to ensure it respositions
  // correctly.
  if (IsOverlayShowing()) {
    CloseAndReshowPreselectionBubble();
  }
}

void LensOverlayController::OnHandshakeComplete() {
  CHECK(IsHandshakeComplete());
  // Notify the overlay that the handshake is complete if its initialized.
  if (page_) {
    page_->NotifyHandshakeComplete();
  }

  // Send the suggest inputs to any pending callbacks.
  pending_suggest_inputs_callbacks_.Notify(GetLensSuggestInputs());
}

const GURL& LensOverlayController::GetPageURL() const {
  if (lens::CanSharePageURLWithLensOverlay(pref_service_)) {
    return tab_->GetContents()->GetVisibleURL();
  }
  return GURL::EmptyGURL();
}

SessionID LensOverlayController::GetTabId() const {
  return sessions::SessionTabHelper::IdForTab(tab_->GetContents());
}

metrics::OmniboxEventProto::PageClassification
LensOverlayController::GetPageClassification() const {
  // There are two cases where we are assuming to be in a contextual flow:
  // 1) We are in the zero state with the overlay CSB showing.
  // 2) A user has made a contextual query and the live page is now showing.
  if (state_ == State::kLivePageAndResults || state_ == State::kOverlay) {
    return metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX;
  }
  return selected_region_thumbnail_uri_.empty()
             ? metrics::OmniboxEventProto::SEARCH_SIDE_PANEL_SEARCHBOX
             : metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX;
}

std::string& LensOverlayController::GetThumbnail() {
  return selected_region_thumbnail_uri_;
}

const lens::proto::LensOverlaySuggestInputs&
LensOverlayController::GetLensSuggestInputs() const {
  if (!initialization_data_ && !pre_initialization_suggest_inputs_) {
    return lens::proto::LensOverlaySuggestInputs().default_instance();
  }
  return initialization_data_ ? initialization_data_->suggest_inputs_
                              : pre_initialization_suggest_inputs_.value();
}

void LensOverlayController::OnTextModified() {
  if (initialization_data_->selected_text_.has_value()) {
    initialization_data_->selected_text_.reset();
    page_->ClearTextSelection();
  }
}

void LensOverlayController::ClearRegionSelection() {
  selected_region_thumbnail_uri_.clear();
  lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
  initialization_data_->selected_region_.reset();
  initialization_data_->selected_region_bitmap_.reset();
  page_->ClearRegionSelection();
}

void LensOverlayController::OnThumbnailRemoved() {
  // This is called by the searchbox after the thumbnail is
  // removed from there, so no need to manually replace the
  // searchbox thumbnail uri here.
  ClearRegionSelection();
}

void LensOverlayController::OnSuggestionAccepted(
    const GURL& destination_url,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion) {
  std::string query_text = "";
  std::map<std::string, std::string> additional_query_parameters;

  net::QueryIterator query_iterator(destination_url);
  while (!query_iterator.IsAtEnd()) {
    std::string_view key = query_iterator.GetKey();
    std::string_view value = query_iterator.GetUnescapedValue();
    if (kTextQueryParameterKey == key) {
      query_text = value;
    } else {
      additional_query_parameters.insert(std::make_pair(
          query_iterator.GetKey(), query_iterator.GetUnescapedValue()));
    }
    query_iterator.Advance();
  }

  IssueSearchBoxRequest(query_text, match_type, is_zero_prefix_suggestion,
                        additional_query_parameters);
}

void LensOverlayController::OnFocusChanged(bool focused) {
  if (!focused) {
    return;
  }

  if (IsContextualSearchbox()) {
    if (!csb_session_end_metrics_.searchbox_focused_) {
      // This is the first time the searchbox is focused in this session.
      // Record the time between the overlay being invoked and the searchbox
      // being focused.
      lens::RecordContextualSearchboxTimeToFirstFocus(
          base::TimeTicks::Now() - invocation_time_,
          initialization_data_->primary_content_type_);
    } else {
      RecordContextualSearchboxTimeToFocusAfterNavigation();
    }
    csb_session_end_metrics_.searchbox_focused_ = true;

    if (state() == State::kLivePageAndResults) {
      // If the live page is showing and the searchbox becomes focused, showing
      // intent to issue a new query, upload the new page content for
      // contextualization.
      TryUpdatePageContextualization();
    }
  }
}

void LensOverlayController::OnPageBound() {
  // If the side panel closes before the remote gets bound,
  // side_panel_searchbox_handler_ could become unset. Verify it is set before
  // sending to the side panel.
  if (!side_panel_searchbox_handler_ ||
      !side_panel_searchbox_handler_->IsRemoteBound()) {
    return;
  }

  // Send any pending inputs for the searchbox.
  if (pending_text_query_.has_value()) {
    side_panel_searchbox_handler_->SetInputText(*pending_text_query_);
    pending_text_query_.reset();
  }
  if (pending_thumbnail_uri_.has_value()) {
    SetSearchboxThumbnail(*pending_thumbnail_uri_);
    pending_thumbnail_uri_.reset();
  }
}

void LensOverlayController::ShowGhostLoaderErrorState() {
  if (!IsContextualSearchbox()) {
    return;
  }
  if (overlay_ghost_loader_page_) {
    overlay_ghost_loader_page_->ShowErrorState();
  }
  if (side_panel_ghost_loader_page_) {
    side_panel_ghost_loader_page_->ShowErrorState();
  }
}

void LensOverlayController::OnZeroSuggestShown() {
  if (!IsContextualSearchbox()) {
    return;
  }

  if (state() == State::kOverlay) {
    csb_session_end_metrics_.zps_shown_on_initial_query_ = true;
  } else {
    csb_session_end_metrics_.zps_shown_on_follow_up_query_ = true;
  }
}

base::CallbackListSubscription
LensOverlayController::GetLensSuggestInputsWhenReady(
    LensOverlaySuggestInputsCallback callback) {
  // Exit early if the overlay is either off or going to soon be off.
  if (state() == State::kOff || IsOverlayClosing()) {
    std::move(callback).Run(std::nullopt);
    return {};
  }

  // If the handshake is complete, return the Lens suggest inputs immediately.
  if (IsHandshakeComplete()) {
    std::move(callback).Run(initialization_data_->suggest_inputs_);
    return {};
  }
  return pending_suggest_inputs_callbacks_.Add(std::move(callback));
}

std::optional<std::string> LensOverlayController::GetPageTitle() {
  std::optional<std::string> page_title;
  content::WebContents* active_web_contents = tab_->GetContents();
  if (lens::CanSharePageTitleWithLensOverlay(sync_service_, pref_service_)) {
    page_title = std::make_optional<std::string>(
        base::UTF16ToUTF8(active_web_contents->GetTitle()));
  }
  return page_title;
}

float LensOverlayController::GetUiScaleFactor() {
  int device_scale_factor =
      tab_->GetContents()->GetRenderWidgetHostView()->GetDeviceScaleFactor();
  float page_scale_factor =
      zoom::ZoomController::FromWebContents(tab_->GetContents())
          ->GetZoomPercent() /
      100.0f;
  return device_scale_factor * page_scale_factor;
}

void LensOverlayController::OnSidePanelDidOpen() {
  // If a side panel opens that is not ours, we must close the overlay.
  if (side_panel_coordinator_->GetCurrentEntryId() !=
      SidePanelEntry::Id::kLensOverlayResults) {
    CloseUISync(lens::LensOverlayDismissalSource::kUnexpectedSidePanelOpen);
  }
}

void LensOverlayController::FinishedWaitingForReflow() {
  if (state_ == State::kClosingOpenedSidePanel) {
    // This path is invoked after the user invokes the overlay, but we needed
    // to close the side panel before taking a screenshot. The Side panel is
    // now closed so we can now take the screenshot of the page.
    CaptureScreenshot();
  }
}

void LensOverlayController::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  // Exit early if the overlay is already closing.
  if (IsOverlayClosing()) {
    return;
  }

  // The overlay's primary main frame process has exited, either cleanly or
  // unexpectedly. Close the overlay so that the user does not get into a broken
  // state where the overlay cannot be dismissed. Note that RenderProcessExited
  // can be called during the destruction of a frame in the overlay, so it is
  // important to post a task to close the overlay to avoid double-freeing the
  // overlay's frames. See https://crbug.com/371643466.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LensOverlayController::CloseUISync, weak_factory_.GetWeakPtr(),
          info.status == base::TERMINATION_STATUS_NORMAL_TERMINATION
              ? lens::LensOverlayDismissalSource::kOverlayRendererClosedNormally
              : lens::LensOverlayDismissalSource::
                    kOverlayRendererClosedUnexpectedly));
}

void LensOverlayController::TabForegrounded(tabs::TabInterface* tab) {
  // Ignore the event if the overlay is not backgrounded.
  if (state_ != State::kBackground) {
    return;
  }

  // If the overlay was backgrounded, restore the previous state.
  if (backgrounded_state_ != State::kLivePageAndResults) {
    ShowOverlay();
  }
  if (backgrounded_state_ != State::kOverlayAndResults &&
      backgrounded_state_ != State::kLivePageAndResults) {
    ShowPreselectionBubble();
  }
  if (lens::features::IsLensOverlayContextualSearchboxEnabled()) {
    SuppressGhostLoader();
  }

  state_ = backgrounded_state_;
  UpdateEntryPointsState();
}

void LensOverlayController::TabWillEnterBackground(tabs::TabInterface* tab) {
  // If the current tab was already backgrounded, do nothing.
  if (state_ == State::kBackground) {
    return;
  }

  // If the overlay is active, background it.
  if (IsOverlayActive()) {
    // If the overlay is currently showing, then we should hide the UI.
    if (IsOverlayShowing()) {
      HideOverlay();
    }

    backgrounded_state_ = state_;
    state_ = State::kBackground;
    UpdateEntryPointsState();

    // TODO(crbug.com/335516480): Schedule the UI to be suspended.
    return;
  }

  // This is still possible when the controller is in state kScreenshot and the
  // tab was backgrounded. We should close the UI as the overlay has not been
  // created yet.
  CloseUISync(
      lens::LensOverlayDismissalSource::kTabBackgroundedWhileScreenshotting);
}

void LensOverlayController::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  // Background tab contents discarded.
  CloseUISync(lens::LensOverlayDismissalSource::kTabContentsDiscarded);
}

void LensOverlayController::WillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  // When dragging a tab into a new window, all window-specific state must be
  // reset. As this flow is not fully functional, close the overlay regardless
  // of `reason`. https://crbug.com/342921671.
  switch (reason) {
    case tabs::TabInterface::DetachReason::kDelete:
      CloseUISync(lens::LensOverlayDismissalSource::kTabClosed);
      return;
    case tabs::TabInterface::DetachReason::kInsertIntoOtherWindow:
      CloseUISync(lens::LensOverlayDismissalSource::kTabDragNewWindow);
      return;
  }
}

void LensOverlayController::ActivityRequestedByOverlay(
    ui::mojom::ClickModifiersPtr click_modifiers) {
  // The tab is expected to be in the foreground.
  if (!tab_->IsActivated()) {
    return;
  }
  tab_->GetBrowserWindowInterface()->OpenGURL(
      GURL(lens::features::GetLensOverlayActivityURL()),
      ui::DispositionFromClick(
          click_modifiers->middle_button, click_modifiers->alt_key,
          click_modifiers->ctrl_key, click_modifiers->meta_key,
          click_modifiers->shift_key,
          WindowOpenDisposition::NEW_FOREGROUND_TAB));
}

void LensOverlayController::ActivityRequestedByEvent(int event_flags) {
  // The tab is expected to be in the foreground.
  if (!tab_->IsActivated()) {
    return;
  }
  tab_->GetBrowserWindowInterface()->OpenGURL(
      GURL(lens::features::GetLensOverlayActivityURL()),
      ui::DispositionFromEventFlags(event_flags,
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB));
}

void LensOverlayController::AddBackgroundBlur() {
  // We do not blur unless the overlay is currently active and the blur delegate
  // was created.
  if (!lens_overlay_blur_layer_delegate_ ||
      (state_ != State::kOverlay && state_ != State::kOverlayAndResults)) {
    return;
  }

  // Add our blur layer to the view.
  overlay_web_view_->SetPaintToLayer();
  overlay_web_view_->layer()->Add(lens_overlay_blur_layer_delegate_->layer());
  overlay_web_view_->layer()->StackAtBottom(
      lens_overlay_blur_layer_delegate_->layer());
  lens_overlay_blur_layer_delegate_->layer()->SetBounds(
      overlay_web_view_->GetLocalBounds());
}

void LensOverlayController::CloseRequestedByOverlayCloseButton() {
  CloseUIAsync(lens::LensOverlayDismissalSource::kOverlayCloseButton);
}

void LensOverlayController::CloseRequestedByOverlayBackgroundClick() {
  CloseUIAsync(lens::LensOverlayDismissalSource::kOverlayBackgroundClick);
}

void LensOverlayController::FeedbackRequestedByOverlay() {
  chrome::ShowFeedbackPage(
      tab_->GetContents()->GetLastCommittedURL(),
      tab_->GetBrowserWindowInterface()->GetProfile(),
      feedback::kFeedbackSourceLensOverlay,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(IDS_LENS_SEND_FEEDBACK_PLACEHOLDER),
      /*category_tag=*/"lens_overlay",
      /*extra_diagnostics=*/std::string());
}

void LensOverlayController::FeedbackRequestedByEvent(int event_flags) {
  FeedbackRequestedByOverlay();
}

void LensOverlayController::GetOverlayInvocationSource(
    GetOverlayInvocationSourceCallback callback) {
  std::move(callback).Run(GetInvocationSourceString());
}

void LensOverlayController::InfoRequestedByOverlay(
    ui::mojom::ClickModifiersPtr click_modifiers) {
  // The tab is expected to be in the foreground.
  if (!tab_->IsActivated()) {
    return;
  }
  tab_->GetBrowserWindowInterface()->OpenGURL(
      GURL(lens::features::GetLensOverlayHelpCenterURL()),
      ui::DispositionFromClick(
          click_modifiers->middle_button, click_modifiers->alt_key,
          click_modifiers->ctrl_key, click_modifiers->meta_key,
          click_modifiers->shift_key,
          WindowOpenDisposition::NEW_FOREGROUND_TAB));
}

void LensOverlayController::InfoRequestedByEvent(int event_flags) {
  // The tab is expected to be in the foreground.
  if (!tab_->IsActivated()) {
    return;
  }
  tab_->GetBrowserWindowInterface()->OpenGURL(
      GURL(lens::features::GetLensOverlayHelpCenterURL()),
      ui::DispositionFromEventFlags(event_flags,
                                    WindowOpenDisposition::NEW_FOREGROUND_TAB));
}

void LensOverlayController::IssueLensRegionRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    bool is_click) {
  IssueLensRequest(std::move(region),
                   is_click ? lens::TAP_ON_EMPTY : lens::REGION_SEARCH,
                   std::nullopt);
}

void LensOverlayController::IssueLensObjectRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    bool is_mask_click) {
  IssueLensRequest(
      std::move(region),
      is_mask_click ? lens::TAP_ON_REGION_GLEAM : lens::TAP_ON_OBJECT,
      std::nullopt);
}

void LensOverlayController::IssueTextSelectionRequest(const std::string& query,
                                                      int selection_start_index,
                                                      int selection_end_index,
                                                      bool is_translate) {
  initialization_data_->additional_search_query_params_.clear();
  lens_selection_type_ =
      is_translate ? lens::SELECT_TRANSLATED_TEXT : lens::SELECT_TEXT_HIGHLIGHT;

  IssueTextSelectionRequestInner(query, selection_start_index,
                                 selection_end_index);
}

void LensOverlayController::IssueTranslateSelectionRequest(
    const std::string& query,
    const std::string& content_language,
    int selection_start_index,
    int selection_end_index) {
  initialization_data_->additional_search_query_params_.clear();
  lens::AppendTranslateParamsToMap(
      initialization_data_->additional_search_query_params_, query, "auto");
  lens_selection_type_ = lens::TRANSLATE_CHIP;

  IssueTextSelectionRequestInner(query, selection_start_index,
                                 selection_end_index);
}

void LensOverlayController::IssueMathSelectionRequest(
    const std::string& query,
    const std::string& formula,
    int selection_start_index,
    int selection_end_index) {
  initialization_data_->additional_search_query_params_.clear();
  lens::AppendStickinessSignalForFormula(
      initialization_data_->additional_search_query_params_, formula);
  lens_selection_type_ = lens::SYMBOLIC_MATH_OBJECT;

  IssueTextSelectionRequestInner(query, selection_start_index,
                                 selection_end_index);
}

void LensOverlayController::IssueTextSelectionRequestInner(
    const std::string& query,
    int selection_start_index,
    int selection_end_index) {
  initialization_data_->selected_region_.reset();
  initialization_data_->selected_region_bitmap_.reset();
  selected_region_thumbnail_uri_.clear();
  initialization_data_->selected_text_ =
      std::make_pair(selection_start_index, selection_end_index);

  SetSearchboxInputText(query);
  SetSearchboxThumbnail(std::string());

  lens_overlay_query_controller_->SendTextOnlyQuery(
      query, lens_selection_type_,
      initialization_data_->additional_search_query_params_);
  MaybeOpenSidePanel();
  RecordTimeToFirstInteraction(
      lens::LensOverlayFirstInteractionType::kTextSelect);
  state_ = State::kOverlayAndResults;
  MaybeLaunchSurvey();
}

void LensOverlayController::ClosePreselectionBubble() {
  if (preselection_widget_) {
    preselection_widget_->Close();
    preselection_widget_ = nullptr;
    preselection_widget_observer_.Reset();
  }
}

void LensOverlayController::ShowPreselectionBubble() {
  // Don't show the preselection bubble if the overlay is not being shown.
  if (!should_show_overlay_ || state() == State::kOverlayAndResults) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  // On Mac, the kShowFullscreenToolbar pref is used to determine whether the
  // toolbar is always shown. This causes the toolbar to never unreveal, meaning
  // the preselection bubble will never be shown. Check for this case and show
  // the preselection bubble if needed.
  const bool always_show_toolbar =
      pref_service_->GetBoolean(prefs::kShowFullscreenToolbar);
#else
  const bool always_show_toolbar = false;
#endif  // BUILDFLAG(IS_MAC)

  if (!always_show_toolbar && tab_->GetBrowserWindowInterface()
                                  ->GetImmersiveModeController()
                                  ->IsRevealed()) {
    // If the immersive mode controller is revealing top chrome, do not show
    // the preselection bubble. The bubble will be shown once the reveal
    // finishes.
    return;
  }

  if (!preselection_widget_) {
    CHECK(preselection_widget_anchor_);
    // Setup the preselection widget.
    preselection_widget_ = views::BubbleDialogDelegateView::CreateBubble(
        std::make_unique<lens::LensPreselectionBubble>(
            weak_factory_.GetWeakPtr(), preselection_widget_anchor_,
            net::NetworkChangeNotifier::IsOffline(),
            /*exit_clicked_callback=*/
            base::BindRepeating(
                &LensOverlayController::CloseUIAsync,
                weak_factory_.GetWeakPtr(),
                lens::LensOverlayDismissalSource::kPreselectionToastExitButton),
            /*on_cancel_callback=*/
            base::BindOnce(&LensOverlayController::CloseUIAsync,
                           weak_factory_.GetWeakPtr(),
                           lens::LensOverlayDismissalSource::
                               kPreselectionToastEscapeKeyPress)));
    preselection_widget_->SetNativeWindowProperty(
        views::kWidgetIdentifierKey,
        const_cast<void*>(kLensOverlayPreselectionWidgetIdentifier));
    preselection_widget_observer_.Observe(preselection_widget_);
    // Setting the parent allows focus traversal out of the preselection widget.
    preselection_widget_->SetFocusTraversableParent(
        preselection_widget_anchor_->GetWidget()->GetFocusTraversable());
    preselection_widget_->SetFocusTraversableParentView(
        preselection_widget_anchor_);
  }

  // When in fullscreen, top Chrome may cover this widget on Mac. Set the
  // z-order to floating UI element to ensure the widget is above the top
  // Chrome. Only do this if immersive mode is enabled to avoid issues with
  // the preselection widget covering other windows.
  if (tab_->GetBrowserWindowInterface()
          ->GetImmersiveModeController()
          ->IsEnabled()) {
    preselection_widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingUIElement);
  } else {
    preselection_widget_->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  }

  auto* bubble_view = static_cast<lens::LensPreselectionBubble*>(
      preselection_widget_->widget_delegate());
  bubble_view->SetCanActivate(true);

  // The bubble position is dependent on if top chrome is showing. Resize the
  // bubble to ensure the correct position is used.
  bubble_view->SizeToContents();
  // Show inactive so that the overlay remains active.
  preselection_widget_->ShowInactive();
}

void LensOverlayController::CloseAndReshowPreselectionBubble() {
  // If the preselection bubble is already closed, do not reshow it.
  if (!preselection_widget_) {
    return;
  }
  ClosePreselectionBubble();
  ShowPreselectionBubble();
}

void LensOverlayController::HidePreselectionBubble() {
  if (preselection_widget_) {
    // The preselection bubble remains in the browser's focus order even when it
    // is hidden, for example, when another browser tab is active. This means it
    // remains possible for the bubble to be activated by keyboard input i.e.
    // tabbing into the bubble, which unhides the bubble even on a browser tab
    // where the overlay is not being shown. Prevent this by setting the bubble
    // to non-activatable while it is hidden.
    auto* bubble_view = static_cast<lens::LensPreselectionBubble*>(
        preselection_widget_->widget_delegate());
    bubble_view->SetCanActivate(false);

    preselection_widget_->Hide();
  }
}

void LensOverlayController::IssueSearchBoxRequest(
    const std::string& search_box_text,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion,
    std::map<std::string, std::string> additional_query_params) {
  // Log the interaction time here so the time to fetch new page bytes is not
  // intcluded.
  RecordContextualSearchboxTimeToInteractionAfterNavigation();
  RecordTimeToFirstInteraction(
      lens::LensOverlayFirstInteractionType::kSearchbox);

  // Do not attempt to contextualize if CSB is disabled, if recontextualization
  // on each query is disabled, if the live page is not being displayed, or if
  // the user is not in the contextual search flow (aka, issues an image request
  // already).
  if (!lens::features::IsLensOverlayContextualSearchboxEnabled() ||
      !lens::features::ShouldLensOverlayRecontextualizeOnQuery() ||
      state() != State::kLivePageAndResults || !IsContextualSearchbox()) {
    IssueSearchBoxRequestPart2(search_box_text, match_type,
                               is_zero_prefix_suggestion,
                               additional_query_params);
    return;
  }

  // If contextual searchbox is enabled, make sure the page bytes are current
  // prior to issuing the search box request.
  GetPageContextualization(
      base::BindOnce(&LensOverlayController::UpdatePageContextualization,
                     weak_factory_.GetWeakPtr())
          .Then(base::BindOnce(
              &LensOverlayController::IssueSearchBoxRequestPart2,
              weak_factory_.GetWeakPtr(), search_box_text, match_type,
              is_zero_prefix_suggestion, additional_query_params)));
}

void LensOverlayController::IssueSearchBoxRequestPart2(
    const std::string& search_box_text,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion,
    std::map<std::string, std::string> additional_query_params) {
  // If the overlay is closing or is off, do not attempt to issue the query.
  if (IsOverlayClosing() || state() == State::kOff) {
    return;
  }
  initialization_data_->additional_search_query_params_ =
      additional_query_params;

  if (initialization_data_->selected_region_.is_null() &&
      GetPageClassification() ==
          metrics::OmniboxEventProto::SEARCH_SIDE_PANEL_SEARCHBOX) {
    // Non-Lens and non-contextual searches should not have a selection type.
    lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
  } else if (is_zero_prefix_suggestion) {
    lens_selection_type_ = lens::MULTIMODAL_SUGGEST_ZERO_PREFIX;
  } else if (match_type == AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED) {
    lens_selection_type_ = lens::MULTIMODAL_SEARCH;
  } else {
    lens_selection_type_ = lens::MULTIMODAL_SUGGEST_TYPEAHEAD;
  }

  if (initialization_data_->selected_region_.is_null() &&
      IsContextualSearchbox()) {
    lens_overlay_query_controller_->SendContextualTextQuery(
        search_box_text, lens_selection_type_,
        initialization_data_->additional_search_query_params_);
    csb_session_end_metrics_.zps_used_ =
        csb_session_end_metrics_.zps_used_ || is_zero_prefix_suggestion;
    csb_session_end_metrics_.query_issued_ = true;
    if (state() == State::kLivePageAndResults) {
      csb_session_end_metrics_.follow_up_query_issued_ = true;
    }
    if (state() == State::kOverlay &&
        !csb_session_end_metrics_.zps_shown_on_initial_query_) {
      // If the query was made in the initial state, and the ZPS has not been
      // shown, mark the query as issued before ZPS shown.
      csb_session_end_metrics_.initial_query_issued_before_zps_shown_ = true;
    } else if (state() == State::kLivePageAndResults &&
               !csb_session_end_metrics_.zps_shown_on_follow_up_query_) {
      // If a follow up query was made, and the ZPS has not been
      // shown for the follow up query, mark the query as issued before ZPS
      // shown.
      csb_session_end_metrics_.follow_up_query_issued_before_zps_shown_ = true;
    }
  } else if (initialization_data_->selected_region_.is_null()) {
    lens_overlay_query_controller_->SendTextOnlyQuery(
        search_box_text, lens_selection_type_,
        initialization_data_->additional_search_query_params_);
  } else {
    std::optional<SkBitmap> selected_region_bitmap =
        initialization_data_->selected_region_bitmap_.drawsNothing()
            ? std::nullopt
            : std::make_optional<SkBitmap>(
                  initialization_data_->selected_region_bitmap_);
    lens_overlay_query_controller_->SendMultimodalRequest(
        initialization_data_->selected_region_.Clone(), search_box_text,
        lens_selection_type_,
        initialization_data_->additional_search_query_params_,
        selected_region_bitmap);
  }

  // If we are in the zero state, this request must have come from CSB. In that
  // case, hide the overlay to allow live page to show through.
  if (state_ == State::kOverlay) {
    HideOverlay();
  }

  // If this a search query from the side panel search box with the overlay
  // showing, keep the state as kOverlayAndResults. Else, we are in our
  // contextual flow and the state needs to stay as State::kLivePageAndResults.
  state_ = state_ == State::kOverlayAndResults ? State::kOverlayAndResults
                                               : State::kLivePageAndResults;

  // The searchbox text is set once the URL loads in the results frame, however,
  // adding it here allows the user to see the text query in the searchbox while
  // a long query loads.
  SetSearchboxInputText(search_box_text);

  MaybeOpenSidePanel();
  results_side_panel_coordinator_->SetSidePanelIsLoadingResults(true);
  MaybeLaunchSurvey();

  // After the searchbox request is sent, mark the follow up zps as not shown so
  // it is false for the next follow up query.
  csb_session_end_metrics_.zps_shown_on_follow_up_query_ = false;
}

void LensOverlayController::HandleStartQueryResponse(
    std::vector<lens::mojom::OverlayObjectPtr> objects,
    lens::mojom::TextPtr text,
    bool is_error) {
  // If the side panel is open, then the error page state can change depending
  // on whether the query succeeded or not. If the side panel is not open, the
  // error page state can only change if the query failed since the first side
  // panel navigation will take care of recording whether the result was shown
  const bool is_side_panel_open =
      results_side_panel_coordinator_->IsSidePanelBound();
  if (is_side_panel_open) {
    results_side_panel_coordinator_->MaybeSetSidePanelShowErrorPage(
        is_error,
        is_error ? lens::SidePanelResultStatus::kErrorPageShownStartQueryError
                 : lens::SidePanelResultStatus::kResultShown);
  } else if (!is_side_panel_open && is_error) {
    results_side_panel_coordinator_->MaybeSetSidePanelShowErrorPage(
        /*should_show_error_page=*/true,
        lens::SidePanelResultStatus::kErrorPageShownStartQueryError);
  }

  if (!objects.empty()) {
    SendObjects(std::move(objects));
  }

  // Text can be null if there was no text within the server response.
  if (!text.is_null()) {
    // If the initialization data is not yet ready, SendText will store the text
    // to be attached when ready.
    if (initialization_data_) {
      initialization_data_->text_ = text.Clone();
    }

    SendText(std::move(text));

    // Try and record the OCR DOM similarity since the OCR text is now
    // available.
    TryCalculateAndRecordOcrDomSimilarity();
  }
}

void LensOverlayController::HandleInteractionURLResponse(
    lens::proto::LensOverlayUrlResponse response) {
  MaybeOpenSidePanel();
  results_side_panel_coordinator_->LoadURLInResultsFrame(GURL(response.url()));
}

void LensOverlayController::HandleInteractionResponse(
    lens::mojom::TextPtr text) {
  SendText(std::move(text));
}

void LensOverlayController::HandleSuggestInputsResponse(
    lens::proto::LensOverlaySuggestInputs suggest_inputs) {
  if (!initialization_data_) {
    // If the initialization data is not ready, store the suggest inputs to be
    // attached to the initialization data when it is ready.
    pre_initialization_suggest_inputs_ = std::make_optional(suggest_inputs);
    return;
  }

  // If the handshake was already complete, without the new suggest inputs,
  // exit early so that we do not notify OnHandshakeComplete() multiple times.
  if (IsHandshakeComplete()) {
    initialization_data_->suggest_inputs_ = suggest_inputs;
    return;
  }

  // Check if the handshake with the server has been completed with the new
  // inputs. If so, this is the first time we are receiving the suggest inputs,
  // so we need to notify OnHandshakeComplete() to allow the searchbox to query
  // autocomplete.
  initialization_data_->suggest_inputs_ = suggest_inputs;
  if (IsHandshakeComplete()) {
    // Notify the overlay that it is now safe to query autocomplete.
    OnHandshakeComplete();
  }
}

void LensOverlayController::HandlePageContentUploadProgress(uint64_t position,
                                                            uint64_t total) {
  // If the progress bar is disabled, do not show it.
  if (!lens::features::ShouldShowUploadProgressBar() ||
      !is_upload_progress_bar_shown_ || !IsContextualSearchbox()) {
    return;
  }

  float progress = total > 0 ? static_cast<float>(position) / total : 1.0f;

  // For the first upload handler event received, check if the progress is above
  // the heuristic threshold. If so, do not show the progress bar because it is
  // assumed that the upload will finish quickly, and showing the progress bar
  // would be distracting.
  if (is_first_upload_handler_event_) {
    is_first_upload_handler_event_ = false;
    if (total > 0 &&
        progress > lens::features::GetUploadProgressBarShowHeuristic()) {
      is_upload_progress_bar_shown_ = false;
      return;
    }
  }

  results_side_panel_coordinator_->SetPageContentUploadProgress(
      total > 0 ? static_cast<float>(position) / total : 1.0f);
}

void LensOverlayController::HandleThumbnailCreated(
    const std::string& thumbnail_bytes) {
  selected_region_thumbnail_uri_ =
      webui::MakeDataURIForImage(base::as_byte_span(thumbnail_bytes), "jpeg");
  SetSearchboxThumbnail(selected_region_thumbnail_uri_);
}

void LensOverlayController::RecordTimeToFirstInteraction(
    lens::LensOverlayFirstInteractionType interaction_type) {
  if (search_performed_in_session_) {
    return;
  }
  DCHECK(!invocation_time_.is_null());
  base::TimeDelta time_to_first_interaction =
      base::TimeTicks::Now() - invocation_time_;
  ukm::SourceId source_id =
      tab_->GetContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  // UMA and UKM TimeToFirstInteraction.
  lens::RecordTimeToFirstInteraction(invocation_source_,
                                     time_to_first_interaction,
                                     interaction_type, source_id);
  search_performed_in_session_ = true;
}

void LensOverlayController::
    RecordContextualSearchboxTimeToFocusAfterNavigation() {
  if (!last_navigation_time_.has_value() ||
      contextual_searchbox_focused_after_navigation_) {
    return;
  }
  base::TimeDelta time_to_focus =
      base::TimeTicks::Now() - last_navigation_time_.value();
  lens::RecordContextualSearchboxTimeToFocusAfterNavigation(
      time_to_focus, initialization_data_->primary_content_type_);
  contextual_searchbox_focused_after_navigation_ = true;
}

void LensOverlayController::
    RecordContextualSearchboxTimeToInteractionAfterNavigation() {
  if (!last_navigation_time_.has_value()) {
    return;
  }
  base::TimeDelta time_to_interaction =
      base::TimeTicks::Now() - last_navigation_time_.value();
  lens::RecordContextualSearchboxTimeToInteractionAfterNavigation(
      time_to_interaction, initialization_data_->primary_content_type_);
  last_navigation_time_.reset();
}

void LensOverlayController::RecordEndOfSessionMetrics(
    lens::LensOverlayDismissalSource dismissal_source) {
  // UMA unsliced Dismissed.
  lens::RecordDismissal(dismissal_source);

  // UMA InvocationResultedInSearch.
  lens::RecordInvocationResultedInSearch(invocation_source_,
                                         search_performed_in_session_);

  // UMA session duration.
  DCHECK(!invocation_time_.is_null());
  base::TimeDelta session_duration = base::TimeTicks::Now() - invocation_time_;
  lens::RecordSessionDuration(invocation_source_, session_duration);

  // UKM session end metrics. Includes invocation source, whether the
  // session resulted in a search, invocation document type and session
  // duration.
  ukm::SourceId source_id =
      tab_->GetContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  lens::RecordUKMSessionEndMetrics(source_id, invocation_source_,
                                   search_performed_in_session_,
                                   session_duration, initial_document_type_);

  // UMA and UKM end of session metrics for the CSB. Only recorded if CSB is
  // shown in session.
  lens::RecordContextualSearchboxSessionEndMetrics(
      source_id, csb_session_end_metrics_, initial_page_content_type_,
      initial_document_type_);
}

void LensOverlayController::RecordDocumentMetrics(
    std::optional<uint32_t> page_count) {
  // Use the first lens::PageContent as it is the most important PDF/Webpage.
  // TODO(crbug.com/398304347): Ideally all lens::PageContent sizes should be
  // recorded.
  auto content_type =
      initialization_data_->page_contents_.empty()
          ? lens::MimeType::kUnknown
          : initialization_data_->page_contents_.front().content_type_;
  auto page_content_bytes_size =
      initialization_data_->page_contents_.empty()
          ? 0
          : initialization_data_->page_contents_.front().bytes_.size();
  lens::RecordDocumentSizeBytes(content_type, page_content_bytes_size);

  if (page_count.has_value() && content_type == lens::MimeType::kPdf) {
    lens::RecordPdfPageCount(page_count.value());
    return;
  }

  // Fetch and record the other content type for representing the webpage.
  // TODO(crbug.com/398304347): Remove these once both the innerHtml and
  // innerText metrics are recorded as part of the content data.
  auto* render_frame_host = tab_->GetContents()->GetPrimaryMainFrame();
  if (content_type == lens::MimeType::kHtml) {
    // Fetch the innerText to log the size.
    content_extraction::GetInnerText(
        *render_frame_host, /*node_id=*/std::nullopt,
        base::BindOnce(&LensOverlayController::RecordInnerTextSize,
                       weak_factory_.GetWeakPtr()));
  } else if (content_type == lens::MimeType::kPlainText) {
    // Fetch the innerHtml bytes to log the size.
    content_extraction::GetInnerHtml(
        *render_frame_host,
        base::BindOnce(&LensOverlayController::RecordInnerHtmlSize,
                       weak_factory_.GetWeakPtr()));
  }

  // Try and record the OCR DOM similarity since the page content is now
  // available.
  TryCalculateAndRecordOcrDomSimilarity();
}

void LensOverlayController::TryCalculateAndRecordOcrDomSimilarity() {
  // Only record the similarity once per session.
  if (ocr_dom_similarity_recorded_in_session_) {
    return;
  }

  // Exit early if we do not have all the data needed to calculate the
  // similarity.
  if (!initialization_data_ || initialization_data_->text_.is_null() ||
      initialization_data_->page_contents_.empty()) {
    return;
  }

  const auto& page_content_bytes =
      initialization_data_->page_contents_.front().bytes_;

  const auto primary_content_type = initialization_data_->primary_content_type_;
  bool is_dom = primary_content_type == lens::MimeType::kHtml ||
                primary_content_type == lens::MimeType::kPlainText ||
                primary_content_type == lens::MimeType::kAnnotatedPageContent;
  bool is_dom_too_large =
      page_content_bytes.size() > kMaxDomTextLengthForOcrSimilarity;
  bool is_english = initialization_data_->text_->content_language == "en";

  // Exit early if the page content is not from the DOM, the DOM is very large
  // and might bog down the thread, or the page is not in English since the
  // score is not reliable for other languages.
  if (!is_dom || is_dom_too_large || !is_english) {
    // If the page content is not from the HTML DOM, we cannot calculate the
    // similarity, so mark this as true to avoid trying again.
    ocr_dom_similarity_recorded_in_session_ = true;
    return;
  }

  // Post to a background thread to calculate the similarity to avoid slowing
  // down the main thread.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT},
      base::BindOnce(
          &CalculateWordOverlapSimilarity,
          std::string(page_content_bytes.begin(), page_content_bytes.end()),
          initialization_data_->text_.Clone()),
      base::BindOnce(&lens::RecordOcrDomSimilarity));
  ocr_dom_similarity_recorded_in_session_ = true;
}

void LensOverlayController::RecordInnerTextSize(
    std::unique_ptr<content_extraction::InnerTextResult> result) {
  if (!result) {
    return;
  }
  lens::RecordDocumentSizeBytes(lens::MimeType::kPlainText,
                                result->inner_text.size());
}

void LensOverlayController::RecordInnerHtmlSize(
    const std::optional<std::string>& result) {
  if (!result) {
    return;
  }
  lens::RecordDocumentSizeBytes(lens::MimeType::kHtml, result->size());
}

void LensOverlayController::MaybeLaunchSurvey() {
  if (!base::FeatureList::IsEnabled(lens::features::kLensOverlaySurvey)) {
    return;
  }
  if (hats_triggered_in_session_) {
    return;
  }
  HatsService* hats_service = HatsServiceFactory::GetForProfile(
      tab_->GetBrowserWindowInterface()->GetProfile(),
      /*create_if_necessary=*/true);
  if (!hats_service) {
    // HaTS may not be available in e.g. guest profile
    return;
  }
  hats_triggered_in_session_ = true;
  hats_service->LaunchDelayedSurveyForWebContents(
      kHatsSurveyTriggerLensOverlayResults, tab_->GetContents(),
      lens::features::GetLensOverlaySurveyResultsTime().InMilliseconds(),
      /*product_specific_bits_data=*/{},
      /*product_specific_string_data=*/
      {{"ID that's tied to your Google Lens session",
        base::NumberToString(lens_overlay_query_controller_->gen204_id())}});
}

void LensOverlayController::InitializeTutorialIPHUrlMatcher() {
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHLensOverlayFeature)) {
    return;
  }

  tutorial_iph_url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  base::MatcherStringPattern::ID id(0);
  url_matcher::util::AddFiltersWithLimit(
      tutorial_iph_url_matcher_.get(), true, &id,
      JSONArrayToVector(
          feature_engagement::kIPHLensOverlayUrlAllowFilters.Get()),
      &iph_url_filters_);
  url_matcher::util::AddFiltersWithLimit(
      tutorial_iph_url_matcher_.get(), false, &id,
      JSONArrayToVector(
          feature_engagement::kIPHLensOverlayUrlBlockFilters.Get()),
      &iph_url_filters_);

  auto force_allow_url_strings = JSONArrayToVector(
      feature_engagement::kIPHLensOverlayUrlForceAllowedUrlMatchPatterns.Get());
  std::vector<base::MatcherStringPattern> force_allow_url_patterns;
  std::vector<const base::MatcherStringPattern*> force_allow_url_pointers;
  force_allow_url_patterns.reserve(force_allow_url_strings.size());
  force_allow_url_pointers.reserve(force_allow_url_strings.size());
  for (const std::string& entry : force_allow_url_strings) {
    force_allow_url_patterns.emplace_back(entry, ++id);
    force_allow_url_pointers.push_back(&force_allow_url_patterns.back());
  }
  forced_url_matcher_ = std::make_unique<url_matcher::RegexSetMatcher>();
  // Pointers will not be referenced after AddPatterns() completes.
  forced_url_matcher_->AddPatterns(force_allow_url_pointers);

  auto allow_strings = JSONArrayToVector(
      feature_engagement::kIPHLensOverlayUrlPathMatchAllowPatterns.Get());
  std::vector<base::MatcherStringPattern> allow_patterns;
  std::vector<const base::MatcherStringPattern*> allow_pointers;
  allow_patterns.reserve(allow_strings.size());
  allow_pointers.reserve(allow_strings.size());
  for (const std::string& entry : allow_strings) {
    allow_patterns.emplace_back(entry, ++id);
    allow_pointers.push_back(&allow_patterns.back());
  }
  page_path_allow_matcher_ = std::make_unique<url_matcher::RegexSetMatcher>();
  // Pointers will not be referenced after AddPatterns() completes.
  page_path_allow_matcher_->AddPatterns(allow_pointers);

  auto block_strings = JSONArrayToVector(
      feature_engagement::kIPHLensOverlayUrlPathMatchBlockPatterns.Get());
  std::vector<base::MatcherStringPattern> block_patterns;
  std::vector<const base::MatcherStringPattern*> block_pointers;
  block_patterns.reserve(block_strings.size());
  block_pointers.reserve(block_strings.size());
  for (const std::string& entry : block_strings) {
    block_patterns.emplace_back(entry, ++id);
    block_pointers.push_back(&block_patterns.back());
  }
  page_path_block_matcher_ = std::make_unique<url_matcher::RegexSetMatcher>();
  // Pointers will not be referenced after AddPatterns() completes.
  page_path_block_matcher_->AddPatterns(block_pointers);
}

void LensOverlayController::MaybeShowDelayedTutorialIPH(const GURL& url) {
  // If a tutorial IPH was already queued, cancel it.
  tutorial_iph_timer_.Stop();

  if (IsUrlEligibleForTutorialIPH(url)) {
    tutorial_iph_timer_.Start(
        FROM_HERE, feature_engagement::kIPHLensOverlayDelayTime.Get(),
        base::BindOnce(&LensOverlayController::ShowTutorialIPH,
                       weak_factory_.GetWeakPtr()));
  }
}

void LensOverlayController::UpdateNavigationMetrics() {
  last_navigation_time_ = base::TimeTicks::Now();
  contextual_searchbox_focused_after_navigation_ = false;
}

bool LensOverlayController::IsHandshakeComplete() {
  if (!initialization_data_ && !pre_initialization_suggest_inputs_) {
    return false;
  }
  const auto& suggest_inputs = initialization_data_
                                   ? initialization_data_->suggest_inputs_
                                   : pre_initialization_suggest_inputs_;
  return AreLensSuggestInputsReady(suggest_inputs);
}

bool LensOverlayController::IsUrlEligibleForTutorialIPH(const GURL& url) {
  if (!tutorial_iph_url_matcher_) {
    return false;
  }

  // Check if the URL matches any of the allow filters. If it does not,
  // return false immediately as this should not be a shown match.
  auto matches = tutorial_iph_url_matcher_.get()->MatchURL(url);
  if (!matches.size()) {
    return false;
  }

  // Now that the URL is allowed, check if it matches any of the block filters.
  // If it does, return false as to block this URL from showing the IPH.
  for (auto match : matches) {
    // Blocks take precedence over allows.
    if (!iph_url_filters_[match].allow) {
      return false;
    }
  }

  // Now that the URL is an allowed URL, verify the match is not blocked by
  // the block matcher. If it does contain blocked words in its path, return
  // false to prevent the IPH from being shown.
  if (page_path_block_matcher_ && !page_path_block_matcher_->IsEmpty() &&
      page_path_block_matcher_->Match(url.path(), &matches)) {
    return false;
  }

  // Check if the URL matches any of the forced allowed URLs. If it does, return
  // true as this should be a shown match even if the path does not contain an
  // allowlisted pattern (below).
  if (forced_url_matcher_ && !forced_url_matcher_->IsEmpty() &&
      forced_url_matcher_->Match(url.spec(), &matches)) {
    return true;
  }

  // Finally, check if the URL matches any of the allowed patterns. If it
  // doesn't, return false to prevent the IPH from being shown.
  if (page_path_allow_matcher_ && !page_path_allow_matcher_->IsEmpty() &&
      !page_path_allow_matcher_->Match(url.path(), &matches)) {
    return false;
  }

  // Finally if all checks pass, this must be a valid match. I.e.:
  // 1. The URL matches at least one of the allowed URLs.
  // 2. The URL does not match any of the blocked URLs.
  // 3. The URL does not match any of the block path patterns.
  // 4. The URL matches at least one of the allowed path patterns.
  return true;
}

void LensOverlayController::ShowTutorialIPH() {
  if (auto* user_ed =
          tab_->GetBrowserWindowInterface()->GetUserEducationInterface()) {
    user_ed->MaybeShowFeaturePromo(feature_engagement::kIPHLensOverlayFeature);
  }
}

void LensOverlayController::NotifyUserEducationAboutOverlayUsed() {
  if (auto* user_ed =
          tab_->GetBrowserWindowInterface()->GetUserEducationInterface()) {
    user_ed->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHLensOverlayFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }
}

void LensOverlayController::NotifyPageContentUpdated() {
  auto page_content_type = lens::StringMimeTypeToMojoPageContentType(
      tab_->GetContents()->GetContentsMimeType());
  if (page_) {
    page_->PageContentTypeChanged(page_content_type);
  }
  results_side_panel_coordinator_->NotifyPageContentUpdated();
}

void LensOverlayController::UpdateEntryPointsState() {
  tab_->GetBrowserWindowInterface()
      ->GetFeatures()
      .lens_overlay_entry_point_controller()
      ->UpdateEntryPointsState(
          /*hide_toolbar_entrypoint=*/false);
}
