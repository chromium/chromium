// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ENTERPRISE_REPORTING_ENTERPRISE_REPORTING_PAGE_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ENTERPRISE_REPORTING_ENTERPRISE_REPORTING_PAGE_HANDLER_H_

#include <memory>

#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting.mojom.h"
#include "chromeos/dbus/missive/history_tracker.h"
#include "components/reporting/proto/synced/health.pb.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash::reporting {

class EnterpriseReportingPageHandler
    : public enterprise_reporting::mojom::PageHandler,
      public ::reporting::HistoryTracker::Observer {
 public:
  // Create `EnterpriseReportingPageHandler` to be on-thread destructed,
  // in order to make it capable of using weak pointers.
  static std::unique_ptr<EnterpriseReportingPageHandler,
                         base::OnTaskRunnerDeleter>
  Create(
      mojo::PendingReceiver<enterprise_reporting::mojom::PageHandler> receiver,
      mojo::PendingRemote<enterprise_reporting::mojom::Page> page);

  EnterpriseReportingPageHandler(const EnterpriseReportingPageHandler&) =
      delete;
  EnterpriseReportingPageHandler& operator=(
      const EnterpriseReportingPageHandler&) = delete;

  ~EnterpriseReportingPageHandler() override;

  // enterprise_reporting::mojom::PageHandler:
  void RecordDebugState(bool state) override;
  void GetDebugState(GetDebugStateCallback cb) override;
  void GetErpHistoryData(GetErpHistoryDataCallback cb) override;

  // ::reporting::HistoryTracker::Observer:
  void OnNewData(const ::reporting::ERPHealthData& data) const override;

 private:
  // Constructor to be called by `Create` factory only.
  EnterpriseReportingPageHandler(
      mojo::PendingReceiver<enterprise_reporting::mojom::PageHandler> receiver,
      mojo::PendingRemote<enterprise_reporting::mojom::Page> page);

  SEQUENCE_CHECKER(sequence_checker_);
  const scoped_refptr<base::SequencedTaskRunner> sequenced_task_runner_;

  const mojo::Receiver<enterprise_reporting::mojom::PageHandler> receiver_;
  const mojo::Remote<enterprise_reporting::mojom::Page> page_;

  base::WeakPtrFactory<EnterpriseReportingPageHandler> weak_ptr_factory_{this};
};
}  // namespace ash::reporting

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ENTERPRISE_REPORTING_ENTERPRISE_REPORTING_PAGE_HANDLER_H_
