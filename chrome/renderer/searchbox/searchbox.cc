// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/searchbox/searchbox.h"

#include <stddef.h>
#include <stdint.h>

#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/token.h"
#include "base/types/optional_util.h"
#include "chrome/common/search/search.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/renderer/searchbox/searchbox_extension.h"
#include "components/favicon_base/favicon_types.h"
#include "components/favicon_base/favicon_url_parser.h"
#include "components/url_formatter/url_fixer.h"
#include "content/public/renderer/render_frame.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_registry.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_local_frame.h"

namespace {

// The size of the InstantMostVisitedItem cache.
const size_t kMaxInstantMostVisitedItemCacheSize = 100;

// Returns true if items stored in |old_item_id_pairs| and |new_items| are
// equal.
bool AreMostVisitedItemsEqual(
    const std::vector<InstantMostVisitedItemIDPair>& old_item_id_pairs,
    const std::vector<InstantMostVisitedItem>& new_items) {
  if (old_item_id_pairs.size() != new_items.size())
    return false;

  for (size_t i = 0; i < new_items.size(); ++i) {
    if (new_items[i].url != old_item_id_pairs[i].second.url ||
        new_items[i].title != old_item_id_pairs[i].second.title) {
      return false;
    }
  }
  return true;
}

// Helper for SearchBox::GenerateImageURLFromTransientURL().
class SearchBoxIconURLHelper: public SearchBox::IconURLHelper {
 public:
  explicit SearchBoxIconURLHelper(const SearchBox* search_box);
  ~SearchBoxIconURLHelper() override;
  std::string GetMainFrameToken() const override;
  std::string GetURLStringFromRestrictedID(InstantRestrictedID rid) const
      override;

 private:
  raw_ptr<const SearchBox> search_box_;
};

SearchBoxIconURLHelper::SearchBoxIconURLHelper(const SearchBox* search_box)
    : search_box_(search_box) {
}

SearchBoxIconURLHelper::~SearchBoxIconURLHelper() {
}

std::string SearchBoxIconURLHelper::GetMainFrameToken() const {
  return search_box_->render_frame()
      ->GetWebFrame()
      ->GetLocalFrameToken()
      .ToString();
}

std::string SearchBoxIconURLHelper::GetURLStringFromRestrictedID(
    InstantRestrictedID rid) const {
  InstantMostVisitedItem item;
  if (!search_box_->GetMostVisitedItemWithID(rid, &item))
    return std::string();

  return item.url.spec();
}

}  // namespace

namespace internal {  // for testing

// Parses "<frame_token>/<restricted_id>". If successful, assigns
// |*frame_token| := "<frame_token>", |*rid| := "<restricted_id>", and returns
// true.
bool ParseFrameTokenAndRestrictedId(const std::string& id_part,
                                    std::string* frame_token_out,
                                    InstantRestrictedID* rid_out) {
  DCHECK(frame_token_out);
  DCHECK(rid_out);
  // Check that the path is of Most visited item ID form.
  std::vector<std::string_view> tokens = base::SplitStringPiece(
      id_part, "/", base::KEEP_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  if (tokens.size() != 2)
    return false;

  InstantRestrictedID rid;
  std::optional<base::Token> frame_token = base::Token::FromString(tokens[0]);
  if (!frame_token || !base::StringToInt(tokens[1], &rid) || rid < 0) {
    return false;
  }
  *frame_token_out = tokens[0];
  *rid_out = rid;
  return true;
}

// Takes a favicon |url| that looks like:
//
//   chrome-search://favicon/<frame_token>/<restricted_id>
//   chrome-search://favicon/<parameters>/<frame_token>/<restricted_id>
//
// If successful, assigns |*param_part| := "" or "<parameters>/" (note trailing
// slash), |*frame_id| := "<frame_id>", |*rid| := "rid", and returns true.
bool ParseIconRestrictedUrl(const GURL& url,
                            std::string* param_part,
                            std::string* frame_token,
                            InstantRestrictedID* rid) {
  DCHECK(param_part);
  DCHECK(frame_token);
  DCHECK(rid);
  // Strip leading slash.
  std::string raw_path = url.path();
  DCHECK_GT(raw_path.length(), (size_t) 0);
  DCHECK_EQ(raw_path[0], '/');
  raw_path = raw_path.substr(1);

  // Get the starting index of the page URL.
  chrome::ParsedFaviconPath parsed;
  if (!chrome::ParseFaviconPath(
          raw_path, chrome::FaviconUrlFormat::kFaviconLegacy, &parsed)) {
    return false;
  }
  int path_index = parsed.path_index;

  std::string id_part = raw_path.substr(path_index);
  if (!ParseFrameTokenAndRestrictedId(id_part, frame_token, rid)) {
    return false;
  }

  *param_part = raw_path.substr(0, path_index);
  return true;
}

void TranslateIconRestrictedUrl(const GURL& transient_url,
                                const SearchBox::IconURLHelper& helper,
                                GURL* url) {
  std::string params;
  std::string frame_token;
  InstantRestrictedID rid = -1;

  if (!internal::ParseIconRestrictedUrl(transient_url, &params, &frame_token,
                                        &rid) ||
      frame_token != helper.GetMainFrameToken()) {
    *url = GURL(base::StringPrintf("chrome-search://%s/",
                                   chrome::kChromeUIFaviconHost));
  } else {
    std::string item_url = helper.GetURLStringFromRestrictedID(rid);
    *url = GURL(base::StringPrintf("chrome-search://%s/%s%s",
                                   chrome::kChromeUIFaviconHost, params.c_str(),
                                   item_url.c_str()));
  }
}

}  // namespace internal

SearchBox::IconURLHelper::IconURLHelper() = default;

SearchBox::IconURLHelper::~IconURLHelper() = default;

SearchBox::SearchBox(content::RenderFrame* render_frame)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<SearchBox>(render_frame),
      can_run_js_in_renderframe_(false),
      page_seq_no_(0),
      is_focused_(false),
      is_input_in_progress_(false),
      is_key_capture_enabled_(false),
      most_visited_items_cache_(kMaxInstantMostVisitedItemCacheSize),
      has_received_most_visited_(false) {
  // Connect to the embedded search interface in the browser.
  mojo::AssociatedRemote<search::mojom::EmbeddedSearchConnector> connector;
  render_frame->GetRemoteAssociatedInterfaces()->GetInterface(&connector);
  mojo::PendingAssociatedRemote<search::mojom::EmbeddedSearchClient>
      embedded_search_client;
  receiver_.Bind(embedded_search_client.InitWithNewEndpointAndPassReceiver(),
                 render_frame->GetTaskRunner(
                     blink::TaskType::kInternalNavigationAssociated));
  connector->Connect(embedded_search_service_.BindNewEndpointAndPassReceiver(
                         render_frame->GetTaskRunner(
                             blink::TaskType::kInternalNavigationAssociated)),
                     std::move(embedded_search_client));
}

SearchBox::~SearchBox() = default;

void SearchBox::DeleteMostVisitedItem(
    InstantRestrictedID most_visited_item_id) {
  GURL url = GetURLForMostVisitedItem(most_visited_item_id);
  if (!url.is_valid())
    return;
  embedded_search_service_->DeleteMostVisitedItem(page_seq_no_, url);
}

void SearchBox::GenerateImageURLFromTransientURL(const GURL& transient_url,
                                                 GURL* url) const {
  SearchBoxIconURLHelper helper(this);
  internal::TranslateIconRestrictedUrl(transient_url, helper, url);
}

void SearchBox::GetMostVisitedItems(
    std::vector<InstantMostVisitedItemIDPair>* items) const {
  most_visited_items_cache_.GetCurrentItems(items);
}

bool SearchBox::AreMostVisitedItemsAvailable() const {
  return has_received_most_visited_;
}

bool SearchBox::GetMostVisitedItemWithID(
    InstantRestrictedID most_visited_item_id,
    InstantMostVisitedItem* item) const {
  return most_visited_items_cache_.GetItemWithRestrictedID(most_visited_item_id,
                                                           item);
}

const NtpTheme* SearchBox::GetNtpTheme() const {
  return base::OptionalToPtr(theme_);
}

void SearchBox::StartCapturingKeyStrokes() {
  embedded_search_service_->FocusOmnibox(page_seq_no_, OMNIBOX_FOCUS_INVISIBLE);
}

void SearchBox::StopCapturingKeyStrokes() {
  embedded_search_service_->FocusOmnibox(page_seq_no_, OMNIBOX_FOCUS_NONE);
}

void SearchBox::UndoAllMostVisitedDeletions() {
  embedded_search_service_->UndoAllMostVisitedDeletions(page_seq_no_);
}

void SearchBox::UndoMostVisitedDeletion(
    InstantRestrictedID most_visited_item_id) {
  GURL url = GetURLForMostVisitedItem(most_visited_item_id);
  if (!url.is_valid())
    return;
  embedded_search_service_->UndoMostVisitedDeletion(page_seq_no_, url);
}

void SearchBox::SetPageSequenceNumber(int page_seq_no) {
  page_seq_no_ = page_seq_no;
}

void SearchBox::FocusChanged(OmniboxFocusState new_focus_state,
                             OmniboxFocusChangeReason reason) {
  bool key_capture_enabled = new_focus_state == OMNIBOX_FOCUS_INVISIBLE;
  if (key_capture_enabled != is_key_capture_enabled_) {
    // Tell the page if the key capture mode changed unless the focus state
    // changed because of TYPING. This is because in that case, the browser
    // hasn't really stopped capturing key strokes.
    //
    // (More practically, if we don't do this check, the page would receive
    // onkeycapturechange before the corresponding onchange, and the page would
    // have no way of telling whether the keycapturechange happened because of
    // some actual user action or just because they started typing.)
    if (reason != OMNIBOX_FOCUS_CHANGE_TYPING) {
      is_key_capture_enabled_ = key_capture_enabled;
      DVLOG(1) << render_frame() << " KeyCaptureChange";
      if (can_run_js_in_renderframe_) {
        SearchBoxExtension::DispatchKeyCaptureChange(
            render_frame()->GetWebFrame());
      }
    }
  }
  bool is_focused = new_focus_state == OMNIBOX_FOCUS_VISIBLE;
  if (is_focused != is_focused_) {
    is_focused_ = is_focused;
    DVLOG(1) << render_frame() << " FocusChange";
    if (can_run_js_in_renderframe_)
      SearchBoxExtension::DispatchFocusChange(render_frame()->GetWebFrame());
  }
}

void SearchBox::MostVisitedInfoChanged(
    const InstantMostVisitedInfo& most_visited_info) {
  has_received_most_visited_ = true;

  std::vector<InstantMostVisitedItemIDPair> last_known_items;
  GetMostVisitedItems(&last_known_items);

  if (AreMostVisitedItemsEqual(last_known_items, most_visited_info.items)) {
    return;  // Do not send duplicate onmostvisitedchange events.
  }

  most_visited_items_cache_.AddItems(most_visited_info.items);
  if (can_run_js_in_renderframe_) {
    SearchBoxExtension::DispatchMostVisitedChanged(
        render_frame()->GetWebFrame());
  }
}

void SearchBox::SetInputInProgress(bool is_input_in_progress) {
  if (is_input_in_progress_ == is_input_in_progress)
    return;

  is_input_in_progress_ = is_input_in_progress;
  DVLOG(1) << render_frame() << " SetInputInProgress";
  if (can_run_js_in_renderframe_) {
    if (is_input_in_progress_)
      SearchBoxExtension::DispatchInputStart(render_frame()->GetWebFrame());
    else
      SearchBoxExtension::DispatchInputCancel(render_frame()->GetWebFrame());
  }
}

void SearchBox::ThemeChanged(const NtpTheme& theme) {
  // Do not send duplicate notifications.
  if (theme_ == theme)
    return;

  theme_ = theme;
  if (can_run_js_in_renderframe_)
    SearchBoxExtension::DispatchThemeChange(render_frame()->GetWebFrame());
}

GURL SearchBox::GetURLForMostVisitedItem(InstantRestrictedID item_id) const {
  InstantMostVisitedItem item;
  return GetMostVisitedItemWithID(item_id, &item) ? item.url : GURL();
}

void SearchBox::DidCommitProvisionalLoad(ui::PageTransition transition) {
  can_run_js_in_renderframe_ = true;
}

void SearchBox::OnDestruct() {
  delete this;
}
