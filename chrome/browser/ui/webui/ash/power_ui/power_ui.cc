// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/ash/power_ui/power_ui.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/power/power_data_collector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/webui_util.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "content/public/browser/web_ui_message_handler.h"
#include "ui/base/webui/web_ui_util.h"

namespace ash {

namespace {

const char kRequestBatteryChargeDataCallback[] = "requestBatteryChargeData";
const char kRequestCpuIdleDataCallback[] = "requestCpuIdleData";
const char kRequestCpuFreqDataCallback[] = "requestCpuFreqData";

class PowerMessageHandler : public content::WebUIMessageHandler {
 public:
  PowerMessageHandler();
  ~PowerMessageHandler() override;

  // WebUIMessageHandler implementation.
  void RegisterMessages() override;

 private:
  void OnGetBatteryChargeData(const base::Value::List& value);
  void OnGetCpuIdleData(const base::Value::List& value);
  void OnGetCpuFreqData(const base::Value::List& value);
  base::Value::List GetJsStateOccupancyData(
      const std::vector<CpuDataCollector::StateOccupancySampleDeque>& data,
      const std::vector<std::string>& state_names);
  base::Value::List GetJsSystemResumedData();
};

PowerMessageHandler::PowerMessageHandler() {
}

PowerMessageHandler::~PowerMessageHandler() {
}

void PowerMessageHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      kRequestBatteryChargeDataCallback,
      base::BindRepeating(&PowerMessageHandler::OnGetBatteryChargeData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kRequestCpuIdleDataCallback,
      base::BindRepeating(&PowerMessageHandler::OnGetCpuIdleData,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      kRequestCpuFreqDataCallback,
      base::BindRepeating(&PowerMessageHandler::OnGetCpuFreqData,
                          base::Unretained(this)));
}

void PowerMessageHandler::OnGetBatteryChargeData(
    const base::Value::List& args) {
  CHECK_EQ(1u, args.size());

  AllowJavascript();

  const base::circular_deque<PowerDataCollector::PowerSupplySample>&
      power_supply = PowerDataCollector::Get()->power_supply_data();
  base::Value::List js_power_supply_data;
  for (size_t i = 0; i < power_supply.size(); ++i) {
    const PowerDataCollector::PowerSupplySample& sample = power_supply[i];
    base::Value::Dict element;
    element.Set("batteryPercent", sample.battery_percent);
    element.Set("batteryDischargeRate", sample.battery_discharge_rate);
    element.Set("externalPower", sample.external_power);
    element.Set("time", sample.time.InMillisecondsFSinceUnixEpoch());

    js_power_supply_data.Append(std::move(element));
  }

  base::Value::List js_system_resumed_data = GetJsSystemResumedData();

  base::Value::Dict data;
  data.Set("powerSupplyData", std::move(js_power_supply_data));
  data.Set("systemResumedData", std::move(js_system_resumed_data));
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, data);
}

void PowerMessageHandler::OnGetCpuIdleData(const base::Value::List& args) {
  CHECK_EQ(1u, args.size());

  AllowJavascript();

  const CpuDataCollector& cpu_data_collector =
      PowerDataCollector::Get()->cpu_data_collector();

  const std::vector<CpuDataCollector::StateOccupancySampleDeque>& idle_data =
      cpu_data_collector.cpu_idle_state_data();
  const std::vector<std::string>& idle_state_names =
      cpu_data_collector.cpu_idle_state_names();
  base::Value::List js_idle_data =
      GetJsStateOccupancyData(idle_data, idle_state_names);

  base::Value::List js_system_resumed_data = GetJsSystemResumedData();

  base::Value::Dict data;
  data.Set("idleStateData", std::move(js_idle_data));
  data.Set("systemResumedData", std::move(js_system_resumed_data));
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, data);
}

void PowerMessageHandler::OnGetCpuFreqData(const base::Value::List& args) {
  CHECK_EQ(1u, args.size());

  AllowJavascript();

  const CpuDataCollector& cpu_data_collector =
      PowerDataCollector::Get()->cpu_data_collector();

  const std::vector<CpuDataCollector::StateOccupancySampleDeque>& freq_data =
      cpu_data_collector.cpu_freq_state_data();
  const std::vector<std::string>& freq_state_names =
      cpu_data_collector.cpu_freq_state_names();
  base::Value::List js_freq_data =
      GetJsStateOccupancyData(freq_data, freq_state_names);

  base::Value::List js_system_resumed_data = GetJsSystemResumedData();

  base::Value::Dict data;
  data.Set("freqStateData", std::move(js_freq_data));
  data.Set("systemResumedData", std::move(js_system_resumed_data));
  const base::Value& callback_id = args[0];
  ResolveJavascriptCallback(callback_id, data);
}

base::Value::List PowerMessageHandler::GetJsSystemResumedData() {
  base::Value::List data;

  const base::circular_deque<PowerDataCollector::SystemResumedSample>&
      system_resumed = PowerDataCollector::Get()->system_resumed_data();
  for (size_t i = 0; i < system_resumed.size(); ++i) {
    const PowerDataCollector::SystemResumedSample& sample = system_resumed[i];
    base::Value::Dict element;
    element.Set("sleepDuration", sample.sleep_duration.InMillisecondsF());
    element.Set("time", sample.time.InMillisecondsFSinceUnixEpoch());

    data.Append(std::move(element));
  }
  return data;
}

base::Value::List PowerMessageHandler::GetJsStateOccupancyData(
    const std::vector<CpuDataCollector::StateOccupancySampleDeque>& data,
    const std::vector<std::string>& state_names) {
  base::Value::List js_data;
  for (unsigned int cpu = 0; cpu < data.size(); ++cpu) {
    const CpuDataCollector::StateOccupancySampleDeque& sample_deque = data[cpu];
    base::Value::List js_sample_list;
    for (unsigned int i = 0; i < sample_deque.size(); ++i) {
      const CpuDataCollector::StateOccupancySample& sample = sample_deque[i];
      base::Value::Dict js_sample;
      js_sample.Set("time", sample.time.InMillisecondsFSinceUnixEpoch());
      js_sample.Set("cpuOnline", sample.cpu_online);

      base::Value::Dict state_dict;
      for (size_t index = 0; index < sample.time_in_state.size(); ++index) {
        state_dict.Set(state_names[index],
                       sample.time_in_state[index].InMillisecondsF());
      }
      js_sample.Set("timeInState", std::move(state_dict));

      js_sample_list.Append(std::move(js_sample));
    }
    js_data.Append(std::move(js_sample_list));
  }
  return js_data;
}

}  // namespace

PowerUI::PowerUI(content::WebUI* web_ui) : content::WebUIController(web_ui) {
  web_ui->AddMessageHandler(std::make_unique<PowerMessageHandler>());

  content::WebUIDataSource* html = content::WebUIDataSource::CreateAndAdd(
      Profile::FromWebUI(web_ui), chrome::kChromeUIPowerHost);

  static constexpr webui::LocalizedString kStrings[] = {
      {"titleText", IDS_ABOUT_POWER_TITLE},
      {"showButton", IDS_ABOUT_POWER_SHOW_BUTTON},
      {"hideButton", IDS_ABOUT_POWER_HIDE_BUTTON},
      {"reloadButton", IDS_ABOUT_POWER_RELOAD_BUTTON},
      {"notEnoughDataAvailableYet", IDS_ABOUT_POWER_NOT_ENOUGH_DATA},
      {"systemSuspended", IDS_ABOUT_POWER_SYSTEM_SUSPENDED},
      {"invalidData", IDS_ABOUT_POWER_INVALID},
      {"offlineText", IDS_ABOUT_POWER_OFFLINE},
      {"batteryChargeSectionTitle",
       IDS_ABOUT_POWER_BATTERY_CHARGE_SECTION_TITLE},
      {"batteryChargePercentageHeader",
       IDS_ABOUT_POWER_BATTERY_CHARGE_PERCENTAGE_HEADER},
      {"batteryDischargeRateHeader",
       IDS_ABOUT_POWER_BATTERY_DISCHARGE_RATE_HEADER},
      {"dischargeRateLegendText", IDS_ABOUT_POWER_DISCHARGE_RATE_LEGEND_TEXT},
      {"movingAverageLegendText", IDS_ABOUT_POWER_MOVING_AVERAGE_LEGEND_TEXT},
      {"binnedAverageLegendText", IDS_ABOUT_POWER_BINNED_AVERAGE_LEGEND_TEXT},
      {"averageOverText", IDS_ABOUT_POWER_AVERAGE_OVER_TEXT},
      {"samplesText", IDS_ABOUT_POWER_AVERAGE_SAMPLES_TEXT},
      {"cpuIdleSectionTitle", IDS_ABOUT_POWER_CPU_IDLE_SECTION_TITLE},
      {"idleStateOccupancyPercentageHeader",
       IDS_ABOUT_POWER_CPU_IDLE_STATE_OCCUPANCY_PERCENTAGE},
      {"cpuFreqSectionTitle", IDS_ABOUT_POWER_CPU_FREQ_SECTION_TITLE},
      {"frequencyStateOccupancyPercentageHeader",
       IDS_ABOUT_POWER_CPU_FREQ_STATE_OCCUPANCY_PERCENTAGE},
  };
  html->AddLocalizedStrings(kStrings);

  html->UseStringsJs();

  html->AddResourcePath("power.css", IDR_ABOUT_POWER_CSS);
  html->AddResourcePath("power.js", IDR_ABOUT_POWER_JS);
  html->SetDefaultResource(IDR_ABOUT_POWER_HTML);
}

PowerUI::~PowerUI() {
}

}  // namespace ash
