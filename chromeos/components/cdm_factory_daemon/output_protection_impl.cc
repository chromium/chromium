// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/components/cdm_factory_daemon/output_protection_impl.h"

#include <utility>

#include "ash/shell.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "ui/display/manager/display_configurator.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/types/display_constants.h"

namespace chromeos {
namespace {
// Make sure the mapping between the Mojo enums and the Chrome enums do not
// fall out of sync.
#define VALIDATE_ENUM(mojo_type, chrome_type, name)                           \
  static_assert(                                                              \
      static_cast<uint32_t>(cdm::mojom::OutputProtection::mojo_type::name) == \
          display::chrome_type##_##name,                                      \
      #chrome_type "_" #name "value doesn't match")

VALIDATE_ENUM(ProtectionType, CONTENT_PROTECTION_METHOD, NONE);
VALIDATE_ENUM(ProtectionType, CONTENT_PROTECTION_METHOD, HDCP_TYPE_0);
VALIDATE_ENUM(ProtectionType, CONTENT_PROTECTION_METHOD, HDCP_TYPE_1);
VALIDATE_ENUM(LinkType, DISPLAY_CONNECTION_TYPE, NONE);
VALIDATE_ENUM(LinkType, DISPLAY_CONNECTION_TYPE, UNKNOWN);
VALIDATE_ENUM(LinkType, DISPLAY_CONNECTION_TYPE, INTERNAL);
VALIDATE_ENUM(LinkType, DISPLAY_CONNECTION_TYPE, VGA);
VALIDATE_ENUM(LinkType, DISPLAY_CONNECTION_TYPE, HDMI);
VALIDATE_ENUM(LinkType, DISPLAY_CONNECTION_TYPE, DVI);
VALIDATE_ENUM(LinkType, DISPLAY_CONNECTION_TYPE, DISPLAYPORT);
VALIDATE_ENUM(LinkType, DISPLAY_CONNECTION_TYPE, NETWORK);

static_assert(display::DISPLAY_CONNECTION_TYPE_LAST ==
                  display::DISPLAY_CONNECTION_TYPE_NETWORK,
              "DISPLAY_CONNECTION_TYPE_LAST value doesn't match");

constexpr uint32_t kUnprotectableConnectionTypes =
    display::DISPLAY_CONNECTION_TYPE_UNKNOWN |
    display::DISPLAY_CONNECTION_TYPE_VGA |
    display::DISPLAY_CONNECTION_TYPE_NETWORK;

constexpr uint32_t kProtectableConnectionTypes =
    display::DISPLAY_CONNECTION_TYPE_HDMI |
    display::DISPLAY_CONNECTION_TYPE_DVI |
    display::DISPLAY_CONNECTION_TYPE_DISPLAYPORT;

std::vector<int64_t> GetDisplayIdsFromSnapshots(
    const std::vector<raw_ptr<display::DisplaySnapshot, VectorExperimental>>&
        snapshots) {
  std::vector<int64_t> display_ids;
  for (display::DisplaySnapshot* ds : snapshots) {
    display_ids.push_back(ds->display_id());
  }
  return display_ids;
}

cdm::mojom::OutputProtection::ProtectionType ConvertProtection(
    uint32_t protection_mask) {
  // Only return Type 1 if that is the only type active since we want to reflect
  // the overall output security.
  if ((protection_mask & display::kContentProtectionMethodHdcpAll) ==
      display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_1) {
    return cdm::mojom::OutputProtection::ProtectionType::HDCP_TYPE_1;
  } else if (protection_mask & display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_0) {
    return cdm::mojom::OutputProtection::ProtectionType::HDCP_TYPE_0;
  } else {
    return cdm::mojom::OutputProtection::ProtectionType::NONE;
  }
}

class DisplaySystemDelegateImpl
    : public OutputProtectionImpl::DisplaySystemDelegate {
 public:
  DisplaySystemDelegateImpl() {
    display_configurator_ =
        ash::Shell::Get()->display_manager()->configurator();
    DCHECK(display_configurator_);
    content_protection_manager_ =
        display_configurator_->content_protection_manager();
    DCHECK(content_protection_manager_);
  }
  ~DisplaySystemDelegateImpl() override = default;

  void ApplyContentProtection(
      display::ContentProtectionManager::ClientId client_id,
      int64_t display_id,
      uint32_t protection_mask,
      display::ContentProtectionManager::ApplyContentProtectionCallback
          callback) override {
    content_protection_manager_->ApplyContentProtection(
        client_id, display_id, protection_mask, std::move(callback));
  }
  void QueryContentProtection(
      display::ContentProtectionManager::ClientId client_id,
      int64_t display_id,
      display::ContentProtectionManager::QueryContentProtectionCallback
          callback) override {
    content_protection_manager_->QueryContentProtection(client_id, display_id,
                                                        std::move(callback));
  }
  display::ContentProtectionManager::ClientId RegisterClient() override {
    return content_protection_manager_->RegisterClient();
  }
  void UnregisterClient(
      display::ContentProtectionManager::ClientId client_id) override {
    content_protection_manager_->UnregisterClient(client_id);
  }
  const std::vector<raw_ptr<display::DisplaySnapshot, VectorExperimental>>&
  cached_displays() const override {
    return display_configurator_->cached_displays();
  }

 private:
  raw_ptr<display::ContentProtectionManager>
      content_protection_manager_;  // Not owned.
  raw_ptr<display::DisplayConfigurator> display_configurator_;  // Not owned.
};

// These are reported to UMA server. Do not renumber or reuse values.
enum class OutputProtectionStatus {
  kQueried = 0,
  kNoExternalLink = 1,
  kAllExternalLinksProtected = 2,
  // Note: Only add new values immediately before this line.
  kMaxValue = kAllExternalLinksProtected,
};

void ReportOutputProtectionUMA(OutputProtectionStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Media.EME.OutputProtection.PlatformCdm", status);
}

}  // namespace

// static
void OutputProtectionImpl::Create(
    mojo::PendingReceiver<cdm::mojom::OutputProtection> receiver,
    std::unique_ptr<DisplaySystemDelegate> delegate) {
  // This needs to run on the UI thread for its interactions with the display
  // system.
  if (!content::GetUIThreadTaskRunner({})->RunsTasksInCurrentSequence()) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(&OutputProtectionImpl::Create,
                                  std::move(receiver), std::move(delegate)));
    return;
  }
  // This object should destruct when the mojo connection is lost.
  mojo::MakeSelfOwnedReceiver(
      std::make_unique<OutputProtectionImpl>(std::move(delegate)),
      std::move(receiver));
}

OutputProtectionImpl::OutputProtectionImpl(
    std::unique_ptr<DisplaySystemDelegate> delegate)
    : delegate_(std::move(delegate)) {
  if (!delegate_)
    delegate_ = std::make_unique<DisplaySystemDelegateImpl>();
}

OutputProtectionImpl::~OutputProtectionImpl() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (client_id_)
    delegate_->UnregisterClient(client_id_);
}

void OutputProtectionImpl::QueryStatus(QueryStatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!client_id_)
    Initialize();
  if (display_id_list_.empty()) {
    std::move(callback).Run(true, display::DISPLAY_CONNECTION_TYPE_NONE,
                            ProtectionType::NONE);
    return;
  }

  ReportOutputProtectionQuery();

  // We want to copy this since we will manipulate it.
  std::vector<int64_t> remaining_displays = display_id_list_;
  int64_t curr_display_id = remaining_displays.back();
  remaining_displays.pop_back();
  delegate_->QueryContentProtection(
      client_id_, curr_display_id,
      base::BindOnce(&OutputProtectionImpl::QueryStatusCallbackAggregator,
                     weak_factory_.GetWeakPtr(), std::move(remaining_displays),
                     std::move(callback), true,
                     display::DISPLAY_CONNECTION_TYPE_NONE,
                     display::CONTENT_PROTECTION_METHOD_NONE,
                     display::CONTENT_PROTECTION_METHOD_NONE));
}

void OutputProtectionImpl::EnableProtection(ProtectionType desired_protection,
                                            EnableProtectionCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!client_id_)
    Initialize();

  if (display_id_list_.empty()) {
    std::move(callback).Run(true);
    return;
  }

  // We just pass through what the client requests.
  switch (desired_protection) {
    case ProtectionType::HDCP_TYPE_0:
      desired_protection_mask_ = display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_0;
      break;
    case ProtectionType::HDCP_TYPE_1:
      desired_protection_mask_ = display::CONTENT_PROTECTION_METHOD_HDCP_TYPE_1;
      break;
    case ProtectionType::NONE:
      desired_protection_mask_ = display::CONTENT_PROTECTION_METHOD_NONE;
      break;
  }

  // We want to copy this since we will manipulate it.
  std::vector<int64_t> remaining_displays = display_id_list_;
  int64_t curr_display_id = remaining_displays.back();
  remaining_displays.pop_back();
  delegate_->ApplyContentProtection(
      client_id_, curr_display_id, desired_protection_mask_,
      base::BindOnce(&OutputProtectionImpl::EnableProtectionCallbackAggregator,
                     weak_factory_.GetWeakPtr(), std::move(remaining_displays),
                     std::move(callback), true));
}

void OutputProtectionImpl::Initialize() {
  DCHECK(!client_id_);
  // This needs to be setup on the browser thread, so wait to do it until we
  // are on that thread (i.e. don't do it in the constructor).
  client_id_ = delegate_->RegisterClient();
  DCHECK(client_id_);
  display_observer_.emplace(this);
  display_id_list_ = GetDisplayIdsFromSnapshots(delegate_->cached_displays());
}

void OutputProtectionImpl::EnableProtectionCallbackAggregator(
    std::vector<int64_t> remaining_displays,
    EnableProtectionCallback callback,
    bool aggregate_success,
    bool success) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  aggregate_success &= success;
  if (remaining_displays.empty()) {
    std::move(callback).Run(aggregate_success);
    return;
  }
  int64_t curr_display_id = remaining_displays.back();
  remaining_displays.pop_back();
  delegate_->ApplyContentProtection(
      client_id_, curr_display_id, desired_protection_mask_,
      base::BindOnce(&OutputProtectionImpl::EnableProtectionCallbackAggregator,
                     weak_factory_.GetWeakPtr(), std::move(remaining_displays),
                     std::move(callback), aggregate_success));
}

void OutputProtectionImpl::QueryStatusCallbackAggregator(
    std::vector<int64_t> remaining_displays,
    QueryStatusCallback callback,
    bool aggregate_success,
    uint32_t aggregate_link_mask,
    uint32_t aggregate_protection_mask,
    uint32_t aggregate_no_protection_mask,
    bool success,
    uint32_t link_mask,
    uint32_t protection_mask) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  aggregate_success &= success;
  aggregate_link_mask |= link_mask;
  if (link_mask & kUnprotectableConnectionTypes) {
    aggregate_no_protection_mask |= display::kContentProtectionMethodHdcpAll;
  }
  if (link_mask & kProtectableConnectionTypes) {
    aggregate_protection_mask |= protection_mask;
  }
  if (!remaining_displays.empty()) {
    int64_t curr_display_id = remaining_displays.back();
    remaining_displays.pop_back();
    delegate_->QueryContentProtection(
        client_id_, curr_display_id,
        base::BindOnce(
            &OutputProtectionImpl::QueryStatusCallbackAggregator,
            weak_factory_.GetWeakPtr(), std::move(remaining_displays),
            std::move(callback), aggregate_success, aggregate_link_mask,
            aggregate_protection_mask, aggregate_no_protection_mask));
    return;
  }

  if (aggregate_success) {
    ReportOutputProtectionQueryResult(aggregate_link_mask,
                                      aggregate_protection_mask);
  }

  aggregate_protection_mask &= ~aggregate_no_protection_mask;
  std::move(callback).Run(aggregate_success, aggregate_link_mask,
                          ConvertProtection(aggregate_protection_mask));
}

void OutputProtectionImpl::HandleDisplayChange() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  display_id_list_ = GetDisplayIdsFromSnapshots(delegate_->cached_displays());
  if (desired_protection_mask_) {
    // We always reapply content protection on display changes since we affect
    // all displays.
    EnableProtection(ConvertProtection(desired_protection_mask_),
                     base::DoNothing());
  }
}

void OutputProtectionImpl::OnDisplayAdded(const display::Display& display) {
  HandleDisplayChange();
}

void OutputProtectionImpl::OnDisplayMetricsChanged(
    const display::Display& display,
    uint32_t changed_metrics) {
  HandleDisplayChange();
}

void OutputProtectionImpl::OnDisplaysRemoved(
    const display::Displays& removed_displays) {
  HandleDisplayChange();
}

void OutputProtectionImpl::ReportOutputProtectionQuery() {
  if (uma_for_output_protection_query_reported_)
    return;

  ReportOutputProtectionUMA(OutputProtectionStatus::kQueried);
  uma_for_output_protection_query_reported_ = true;
}

void OutputProtectionImpl::ReportOutputProtectionQueryResult(
    uint32_t link_mask,
    uint32_t protection_mask) {
  DCHECK(uma_for_output_protection_query_reported_);

  if (uma_for_output_protection_positive_result_reported_)
    return;

  // Report UMAs for output protection query result.
  uint32_t external_links =
      (link_mask & ~display::DISPLAY_CONNECTION_TYPE_INTERNAL);

  if (!external_links) {
    ReportOutputProtectionUMA(OutputProtectionStatus::kNoExternalLink);
    uma_for_output_protection_positive_result_reported_ = true;
    return;
  }

  bool is_unprotectable_link_connected =
      (external_links & ~kProtectableConnectionTypes) != 0;
  bool is_hdcp_enabled_on_all_protectable_links =
      (protection_mask & desired_protection_mask_) != 0;

  if (!is_unprotectable_link_connected &&
      is_hdcp_enabled_on_all_protectable_links) {
    ReportOutputProtectionUMA(
        OutputProtectionStatus::kAllExternalLinksProtected);
    uma_for_output_protection_positive_result_reported_ = true;
    return;
  }

  // Do not report a negative result because it could be a false negative.
  // Instead, we will calculate number of negatives using the total number of
  // queries and positive results.
}

}  // namespace chromeos
