// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/domain_reliability/context.h"

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/sparse_histogram.h"
#include "base/rand_util.h"
#include "base/values.h"
#include "components/domain_reliability/dispatcher.h"
#include "components/domain_reliability/uploader.h"
#include "components/domain_reliability/util.h"
#include "net/base/net_errors.h"
#include "net/url_request/url_request_context_getter.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace domain_reliability {

// static
const int DomainReliabilityContext::kMaxUploadDepthToSchedule = 1;

DomainReliabilityContext::Factory::~Factory() {
}

// static
const size_t DomainReliabilityContext::kMaxQueuedBeacons = 150;

DomainReliabilityContext::DomainReliabilityContext(
    MockableTime* time,
    const DomainReliabilityScheduler::Params& scheduler_params,
    const std::string& upload_reporter_string,
    const base::TimeTicks* last_network_change_time,
    const UploadAllowedCallback& upload_allowed_callback,
    DomainReliabilityDispatcher* dispatcher,
    DomainReliabilityUploader* uploader,
    std::unique_ptr<const DomainReliabilityConfig> config)
    : config_(std::move(config)),
      time_(time),
      upload_reporter_string_(upload_reporter_string),
      scheduler_(time,
                 config_->collectors.size(),
                 scheduler_params,
                 base::Bind(&DomainReliabilityContext::ScheduleUpload,
                            base::Unretained(this))),
      dispatcher_(dispatcher),
      uploader_(uploader),
      uploading_beacons_size_(0),
      last_network_change_time_(last_network_change_time),
      upload_allowed_callback_(upload_allowed_callback) {}

DomainReliabilityContext::~DomainReliabilityContext() {
  ClearBeacons();
}

void DomainReliabilityContext::OnBeacon(
    std::unique_ptr<DomainReliabilityBeacon> beacon) {
  bool success = (beacon->status == "ok");
  double sample_rate = beacon->details.quic_port_migration_detected
                           ? 1.0
                           : config().GetSampleRate(success);
  if (base::RandDouble() >= sample_rate)
    return;
  beacon->sample_rate = sample_rate;

  // Allow beacons about reports, but don't schedule an upload for more than
  // one layer of recursion, to avoid infinite report loops.
  if (beacon->upload_depth <= kMaxUploadDepthToSchedule)
    scheduler_.OnBeaconAdded();
  beacons_.push_back(std::move(beacon));
  bool should_evict = beacons_.size() > kMaxQueuedBeacons;
  if (should_evict)
    RemoveOldestBeacon();
}

void DomainReliabilityContext::ClearBeacons() {
  beacons_.clear();
  uploading_beacons_size_ = 0;
}

std::unique_ptr<Value> DomainReliabilityContext::GetWebUIData() const {
  DictionaryValue* context_value = new DictionaryValue();

  context_value->SetString("origin", config().origin.spec());
  context_value->SetInteger("beacon_count", static_cast<int>(beacons_.size()));
  context_value->SetInteger("uploading_beacon_count",
      static_cast<int>(uploading_beacons_size_));
  context_value->Set("scheduler", scheduler_.GetWebUIData());

  return std::unique_ptr<Value>(context_value);
}

void DomainReliabilityContext::GetQueuedBeaconsForTesting(
    std::vector<const DomainReliabilityBeacon*>* beacons_out) const {
  DCHECK(this);
  DCHECK(beacons_out);
  beacons_out->clear();
  for (const auto& beacon : beacons_)
    beacons_out->push_back(beacon.get());
}

void DomainReliabilityContext::ScheduleUpload(
    base::TimeDelta min_delay,
    base::TimeDelta max_delay) {
  dispatcher_->ScheduleTask(
      base::Bind(&DomainReliabilityContext::CallUploadAllowedCallback,
                 weak_factory_.GetWeakPtr()),
      min_delay, max_delay);
}

void DomainReliabilityContext::CallUploadAllowedCallback() {
  RemoveExpiredBeacons();
  if (beacons_.empty())
    return;

  upload_allowed_callback_.Run(
      config().origin,
      base::BindOnce(&DomainReliabilityContext::OnUploadAllowedCallbackComplete,
                     weak_factory_.GetWeakPtr()));
}

void DomainReliabilityContext::OnUploadAllowedCallbackComplete(bool allowed) {
  if (allowed)
    StartUpload();
}

void DomainReliabilityContext::StartUpload() {
  RemoveExpiredBeacons();
  if (beacons_.empty())
    return;

  MarkUpload();

  size_t collector_index = scheduler_.OnUploadStart();
  const GURL& collector_url = *config().collectors[collector_index];

  DCHECK(upload_time_.is_null());
  upload_time_ = time_->NowTicks();
  std::string report_json = "{}";
  int max_upload_depth = -1;
  bool wrote = base::JSONWriter::Write(
      *CreateReport(upload_time_,
                    collector_url,
                    &max_upload_depth),
                    &report_json);
  DCHECK(wrote);
  DCHECK_NE(-1, max_upload_depth);

  uploader_->UploadReport(
      report_json,
      max_upload_depth,
      collector_url,
      base::Bind(
          &DomainReliabilityContext::OnUploadComplete,
          weak_factory_.GetWeakPtr()));
}

void DomainReliabilityContext::OnUploadComplete(
    const DomainReliabilityUploader::UploadResult& result) {
  if (result.is_success())
    CommitUpload();
  else
    RollbackUpload();
  base::TimeTicks first_beacon_time = scheduler_.first_beacon_time();
  scheduler_.OnUploadComplete(result);
  UMA_HISTOGRAM_BOOLEAN("DomainReliability.UploadSuccess",
      result.is_success());
  base::TimeTicks now = time_->NowTicks();
  UMA_HISTOGRAM_LONG_TIMES("DomainReliability.UploadLatency",
                           now - first_beacon_time);
  DCHECK(!upload_time_.is_null());
  UMA_HISTOGRAM_MEDIUM_TIMES("DomainReliability.UploadDuration",
                             now - upload_time_);
  last_upload_time_ = upload_time_;
  upload_time_ = base::TimeTicks();
}

std::unique_ptr<const Value> DomainReliabilityContext::CreateReport(
    base::TimeTicks upload_time,
    const GURL& collector_url,
    int* max_upload_depth_out) const {
  int max_upload_depth = 0;

  std::unique_ptr<ListValue> beacons_value(new ListValue());
  for (const auto& beacon : beacons_) {
    beacons_value->Append(beacon->ToValue(upload_time,
                                          *last_network_change_time_,
                                          collector_url,
                                          config().path_prefixes));
    if (beacon->upload_depth > max_upload_depth)
      max_upload_depth = beacon->upload_depth;
  }

  std::unique_ptr<DictionaryValue> report_value(new DictionaryValue());
  report_value->SetString("reporter", upload_reporter_string_);
  report_value->Set("entries", std::move(beacons_value));

  *max_upload_depth_out = max_upload_depth;
  return std::move(report_value);
}

void DomainReliabilityContext::MarkUpload() {
  DCHECK_EQ(0u, uploading_beacons_size_);
  uploading_beacons_size_ = beacons_.size();
  DCHECK_NE(0u, uploading_beacons_size_);
}

void DomainReliabilityContext::CommitUpload() {
  auto begin = beacons_.begin();
  auto end = begin + uploading_beacons_size_;
  beacons_.erase(begin, end);
  DCHECK_NE(0u, uploading_beacons_size_);
  uploading_beacons_size_ = 0;
}

void DomainReliabilityContext::RollbackUpload() {
  DCHECK_NE(0u, uploading_beacons_size_);
  uploading_beacons_size_ = 0;
}

void DomainReliabilityContext::RemoveOldestBeacon() {
  DCHECK(!beacons_.empty());

  DVLOG(1) << "Beacon queue for " << config().origin << " full; "
           << "removing oldest beacon";

  beacons_.pop_front();

  // If that just removed a beacon counted in uploading_beacons_size_, decrement
  // that.
  if (uploading_beacons_size_ > 0)
    --uploading_beacons_size_;
}

void DomainReliabilityContext::RemoveExpiredBeacons() {
  base::TimeTicks now = time_->NowTicks();
  const base::TimeDelta kMaxAge = base::TimeDelta::FromHours(1);
  while (!beacons_.empty() && now - beacons_.front()->start_time >= kMaxAge)
    beacons_.pop_front();
}

}  // namespace domain_reliability
