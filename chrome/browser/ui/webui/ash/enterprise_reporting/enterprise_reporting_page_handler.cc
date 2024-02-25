// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting_page_handler.h"

#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting.mojom.h"
#include "chrome/browser/ui/webui/ash/enterprise_reporting/history_converter.h"
#include "chromeos/dbus/missive/history_tracker.h"
#include "components/reporting/proto/synced/health.pb.h"
#include "components/reporting/proto/synced/status.pb.h"
#include "components/reporting/util/status.h"

namespace ash::reporting {

// static
std::unique_ptr<EnterpriseReportingPageHandler, base::OnTaskRunnerDeleter>
EnterpriseReportingPageHandler::Create(
    mojo::PendingReceiver<enterprise_reporting::mojom::PageHandler> receiver,
    mojo::PendingRemote<enterprise_reporting::mojom::Page> page) {
  // Ensure sequencing for `EnterpriseReportingPageHandler` in order to make it
  // capable of using weak pointers.
  return std::unique_ptr<EnterpriseReportingPageHandler,
                         base::OnTaskRunnerDeleter>(
      new EnterpriseReportingPageHandler(std::move(receiver), std::move(page)),
      base::OnTaskRunnerDeleter(
          base::SequencedTaskRunner::GetCurrentDefault()));
}

EnterpriseReportingPageHandler::EnterpriseReportingPageHandler(
    mojo::PendingReceiver<enterprise_reporting::mojom::PageHandler> receiver,
    mojo::PendingRemote<enterprise_reporting::mojom::Page> page)
    : enterprise_reporting::mojom::PageHandler(),
      sequenced_task_runner_(base::SequencedTaskRunner::GetCurrentDefault()),
      receiver_(this, std::move(receiver)),
      page_(std::move(page)) {
  ::reporting::HistoryTracker::Get()->AddObserver(this);
}

EnterpriseReportingPageHandler::~EnterpriseReportingPageHandler() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  ::reporting::HistoryTracker::Get()->RemoveObserver(this);
}

void EnterpriseReportingPageHandler::RecordDebugState(bool state) {
  ::reporting::HistoryTracker::Get()->set_debug_state(state);
}

void EnterpriseReportingPageHandler::GetDebugState(GetDebugStateCallback cb) {
  std::move(cb).Run(::reporting::HistoryTracker::Get()->debug_state());
}

void EnterpriseReportingPageHandler::GetErpHistoryData(
    GetErpHistoryDataCallback cb) {
  auto convert_cb = base::BindOnce(
      [](GetErpHistoryDataCallback cb, const ::reporting::ERPHealthData& data) {
        std::move(cb).Run(ConvertHistory(data));
      },
      std::move(cb));
  ::reporting::HistoryTracker::Get()->retrieve_data(std::move(convert_cb));
}

void EnterpriseReportingPageHandler::OnNewData(
    const ::reporting::ERPHealthData& data) const {
  // Call `page_->SetErpHistoryData` on the right runner, making conversion
  // before that.
  sequenced_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          [](base::WeakPtr<const EnterpriseReportingPageHandler> self,
             mojo::StructPtr<enterprise_reporting::mojom::ErpHistoryData>
                 history_data) {
            if (!self) {
              return;
            }
            DCHECK_CALLED_ON_VALID_SEQUENCE(self->sequence_checker_);
            self->page_->SetErpHistoryData(std::move(history_data));
          },
          weak_ptr_factory_.GetWeakPtr(), ConvertHistory(data)));
}
}  // namespace ash::reporting
