// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_OUTPUT_PROTECTION_IMPL_H_
#define CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_OUTPUT_PROTECTION_IMPL_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/cdm_factory_daemon/mojom/output_protection.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/display/display_observer.h"
#include "ui/display/manager/content_protection_manager.h"
#include "ui/display/types/display_snapshot.h"

namespace chromeos {

// Provides a Mojo implementation of the OutputProtection interface which then
// calls into the ContentProtectionManger singleton owned by the ash shell.
class COMPONENT_EXPORT(CDM_FACTORY_DAEMON) OutputProtectionImpl
    : public cdm::mojom::OutputProtection,
      public display::DisplayObserver {
 public:
  // Mainly to enable testing, this abstracts out calls that would normally
  // be made into display::ContentProtectionManager, ash::Screen and
  // display::DisplayConfigurator.
  class DisplaySystemDelegate {
   public:
    virtual ~DisplaySystemDelegate() = default;

    // Delegate to display::ContentProtectionManager.
    virtual void ApplyContentProtection(
        display::ContentProtectionManager::ClientId client_id,
        int64_t display_id,
        uint32_t protection_mask,
        display::ContentProtectionManager::ApplyContentProtectionCallback
            callback) = 0;
    virtual void QueryContentProtection(
        display::ContentProtectionManager::ClientId client_id,
        int64_t display_id,
        display::ContentProtectionManager::QueryContentProtectionCallback
            callback) = 0;
    virtual display::ContentProtectionManager::ClientId RegisterClient() = 0;
    virtual void UnregisterClient(
        display::ContentProtectionManager::ClientId client_id) = 0;

    // Delegate to display::DisplayConfigurator.
    virtual const std::vector<
        raw_ptr<display::DisplaySnapshot, VectorExperimental>>&
    cached_displays() const = 0;
  };

  static void Create(
      mojo::PendingReceiver<cdm::mojom::OutputProtection> receiver,
      std::unique_ptr<DisplaySystemDelegate> delegate = nullptr);

  explicit OutputProtectionImpl(
      std::unique_ptr<DisplaySystemDelegate> delegate);

  OutputProtectionImpl(const OutputProtectionImpl&) = delete;
  OutputProtectionImpl& operator=(const OutputProtectionImpl&) = delete;

  ~OutputProtectionImpl() override;

  // chromeos::cdm::mojom::OutputProtection:
  void QueryStatus(QueryStatusCallback callback) override;
  void EnableProtection(
      cdm::mojom::OutputProtection::ProtectionType desired_protection,
      EnableProtectionCallback callback) override;

 private:
  void Initialize();

  // This is used to successively enable protection on all the displays and
  // aggregate the overall result and fire the callback when complete.
  void EnableProtectionCallbackAggregator(
      std::vector<int64_t> remaining_displays,
      EnableProtectionCallback callback,
      bool aggregate_success,
      bool success);

  // This is used to query multiple displays for the status and then aggregate
  // that into one before we invoke the callback.
  void QueryStatusCallbackAggregator(std::vector<int64_t> remaining_displays,
                                     QueryStatusCallback callback,
                                     bool aggregate_success,
                                     uint32_t aggregate_link_mask,
                                     uint32_t aggregate_protection_mask,
                                     uint32_t aggregate_no_protection_mask,
                                     bool success,
                                     uint32_t link_mask,
                                     uint32_t protection_mask);

  void HandleDisplayChange();

  // display::DisplayObserver:
  void OnDisplayAdded(const display::Display& display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t changed_metrics) override;
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;

  // Helper methods to report output protection UMAs.
  void ReportOutputProtectionQuery();
  void ReportOutputProtectionQueryResult(uint32_t link_mask,
                                         uint32_t protection_mask);

  std::unique_ptr<DisplaySystemDelegate> delegate_;
  display::ContentProtectionManager::ClientId client_id_;

  std::optional<display::ScopedOptionalDisplayObserver> display_observer_;

  std::vector<int64_t> display_id_list_;

  uint32_t desired_protection_mask_{0};

  // Tracks whether an output protection query and a positive query result (no
  // unprotected external link) have been reported to UMA.
  bool uma_for_output_protection_query_reported_ = false;
  bool uma_for_output_protection_positive_result_reported_ = false;

  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<OutputProtectionImpl> weak_factory_{this};
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CDM_FACTORY_DAEMON_OUTPUT_PROTECTION_IMPL_H_
