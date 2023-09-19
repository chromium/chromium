// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ASH_ENTERPRISE_REPORTING_HISTORY_CONVERTER_H_
#define CHROME_BROWSER_UI_WEBUI_ASH_ENTERPRISE_REPORTING_HISTORY_CONVERTER_H_

#include "chrome/browser/ui/webui/ash/enterprise_reporting/enterprise_reporting.mojom.h"
#include "chromeos/dbus/missive/history_tracker.h"
#include "components/reporting/proto/synced/health.pb.h"
#include "components/reporting/proto/synced/status.pb.h"

namespace ash::reporting {

mojo::StructPtr<enterprise_reporting::mojom::ErpHistoryData> ConvertHistory(
    const ::reporting::ERPHealthData& data);

}  // namespace ash::reporting

#endif  // CHROME_BROWSER_UI_WEBUI_ASH_ENTERPRISE_REPORTING_HISTORY_CONVERTER_H_
