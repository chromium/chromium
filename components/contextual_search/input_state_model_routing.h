// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_INPUT_STATE_MODEL_ROUTING_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_INPUT_STATE_MODEL_ROUTING_H_

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "third_party/omnibox_proto/model_mode.pb.h"

namespace omnibox {
struct InputState;
}  // namespace omnibox

// Routes a composebox query to AI Mode (AIM) or the Search results page (SRP)
// when `omnibox::kDynamicAimFulfillment` is enabled.
namespace contextual_search {

struct FileInfo;

// URL query params, key to value. An alias, so callers can use
// `std::map<std::string, std::string>` interchangeably.
using UrlParams = std::map<std::string, std::string>;

// Which attachments may go to SRP. Names and values match the `SrpInputMode`
// the server will send, so keep them in sync.
// Modes up to `kAnyCombinationImagesFiles` each include the earlier ones. The
// later modes build on `kAnyCombinationImagesFiles`, so tabs and Drive files
// can move to SRP in either order.
enum class SrpInputMode {
  // No attachments go to SRP.
  kNone = 0,
  // Adds one image.
  kSingleImage = 1,
  // Adds two or more images.
  kMultiImage = 2,
  // Adds files, with or without images.
  kAnyCombinationImagesFiles = 3,
  // Adds tabs to `kAnyCombinationImagesFiles`.
  kAnyCombinationImagesFilesTabs = 4,
  // Adds Drive files to `kAnyCombinationImagesFiles`.
  kAnyCombinationImagesFilesDrive = 5,
  // Adds tabs and Drive files to `kAnyCombinationImagesFiles`.
  kAnyCombinationAllTypes = 6,
};

// Attachment groups allowed on SRP.
struct SrpEligibleAttachments {
  bool single_image = false;
  bool multi_image = false;
  bool files = false;  // Non-image uploads, with any number of images.
  bool tabs = false;
  bool drive = false;
};

// Returns the groups each `mode` allows.
SrpEligibleAttachments EligibleAttachmentsForMode(SrpInputMode mode);

// Parses a mode's name in snake case, e.g. "single_image" or
// "any_combination_images_files_tabs".
std::optional<SrpInputMode> ParseSrpInputMode(std::string_view name);

// Returns the `srp_input_mode` param, or `kSingleImage` if missing or
// invalid.
SrpInputMode SrpInputModeFromFeatureParams();

// Attachment counts by kind.
struct AttachmentCounts {
  int images = 0;
  int files = 0;  // Non-image uploads.
  int tabs = 0;
  int drive = 0;

  friend bool operator==(const AttachmentCounts&,
                         const AttachmentCounts&) = default;
};

// The routing decision to AIM or SRP.
struct FulfillmentDecision {
  FulfillmentDecision();
  FulfillmentDecision(const FulfillmentDecision&);
  FulfillmentDecision& operator=(const FulfillmentDecision&);
  FulfillmentDecision(FulfillmentDecision&&) noexcept;
  FulfillmentDecision& operator=(FulfillmentDecision&&) noexcept;
  ~FulfillmentDecision();

  // Defaults to AIM, today's behavior for every query.
  bool will_fulfill_in_aim = true;
  // Model to show; unspecified for SRP.
  omnibox::ModelMode model = omnibox::ModelMode::MODEL_MODE_UNSPECIFIED;
  // Params for the search URL.
  UrlParams url_params;
};

// Counts attachments by kind, skipping superseded uploads. Adds
// `pending_tab_count` to the tabs.
AttachmentCounts CountAttachments(const std::vector<FileInfo>& file_infos,
                                  size_t pending_tab_count);

// Returns whether the attachments may go to SRP.
bool AttachmentsCanGoToSrp(const AttachmentCounts& attachments,
                           const SrpEligibleAttachments& eligible);

// Routes to AIM if AI Mode is on, there's no server config yet, the tool's
// params include `udm=50` or `nem` (or it has no config), a tool is used with
// attachments (not covered by the SRP input modes), or the attachments can't
// go to SRP. Otherwise routes to SRP with the tool's params. For AIM, adds the
// active (or default) model's params, with `nem` replacing `udm`.
FulfillmentDecision ComputeFulfillment(const omnibox::InputState& state,
                                       bool aim_mode_active,
                                       const AttachmentCounts& attachments,
                                       const SrpEligibleAttachments& eligible,
                                       bool has_valid_config);

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_INPUT_STATE_MODEL_ROUTING_H_
