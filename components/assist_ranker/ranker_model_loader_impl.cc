// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/assist_ranker/ranker_model_loader_impl.h"

#include <utility>

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/important_file_writer.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/string_util.h"
#include "base/task/post_task.h"
#include "base/task_runner_util.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/assist_ranker/proto/ranker_model.pb.h"
#include "components/assist_ranker/ranker_url_fetcher.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace assist_ranker {
namespace {

// The minimum duration, in minutes, between download attempts.
constexpr int kMinRetryDelayMins = 3;

// Suffixes for the various histograms produced by the backend.
const char kWriteTimerHistogram[] = ".Timer.WriteModel";
const char kReadTimerHistogram[] = ".Timer.ReadModel";
const char kDownloadTimerHistogram[] = ".Timer.DownloadModel";
const char kParsetimerHistogram[] = ".Timer.ParseModel";
const char kModelStatusHistogram[] = ".Model.Status";

// Helper function to UMA log a timer histograms.
void RecordTimerHistogram(const std::string& name, base::TimeDelta duration) {
  base::HistogramBase* counter = base::Histogram::FactoryTimeGet(
      name, base::TimeDelta::FromMilliseconds(10),
      base::TimeDelta::FromMilliseconds(200000), 100,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  DCHECK(counter);
  counter->AddTime(duration);
}

// A helper class to produce a scoped timer histogram that supports using a
// non-static-const name.
class MyScopedHistogramTimer {
 public:
  MyScopedHistogramTimer(const base::StringPiece& name)
      : name_(name.begin(), name.end()), start_(base::TimeTicks::Now()) {}

  ~MyScopedHistogramTimer() {
    RecordTimerHistogram(name_, base::TimeTicks::Now() - start_);
  }

 private:
  const std::string name_;
  const base::TimeTicks start_;

  DISALLOW_COPY_AND_ASSIGN(MyScopedHistogramTimer);
};

std::string LoadFromFile(const base::FilePath& model_path) {
  DCHECK(!model_path.empty());
  DVLOG(2) << "Reading data from: " << model_path.value();
  std::string data;
  if (!base::ReadFileToString(model_path, &data) || data.empty()) {
    DVLOG(2) << "Failed to read data from: " << model_path.value();
    data.clear();
  }
  return data;
}

void SaveToFile(const GURL& model_url,
                const base::FilePath& model_path,
                const std::string& model_data,
                const std::string& uma_prefix) {
  DVLOG(2) << "Saving model from '" << model_url << "'' to '"
           << model_path.value() << "'.";
  MyScopedHistogramTimer timer(uma_prefix + kWriteTimerHistogram);
  base::ImportantFileWriter::WriteFileAtomically(model_path, model_data);
}

}  // namespace

RankerModelLoaderImpl::RankerModelLoaderImpl(
    ValidateModelCallback validate_model_cb,
    OnModelAvailableCallback on_model_available_cb,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory,
    base::FilePath model_path,
    GURL model_url,
    std::string uma_prefix)
    : background_task_runner_(base::CreateSequencedTaskRunner(
          {base::ThreadPool(), base::MayBlock(),
           base::TaskPriority::BEST_EFFORT,
           base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN})),
      validate_model_cb_(std::move(validate_model_cb)),
      on_model_available_cb_(std::move(on_model_available_cb)),
      url_loader_factory_(std::move(url_loader_factory)),
      model_path_(std::move(model_path)),
      model_url_(std::move(model_url)),
      uma_prefix_(std::move(uma_prefix)),
      url_fetcher_(std::make_unique<RankerURLFetcher>()) {}

RankerModelLoaderImpl::~RankerModelLoaderImpl() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void RankerModelLoaderImpl::NotifyOfRankerActivity() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  switch (state_) {
    case LoaderState::NOT_STARTED:
      if (!model_path_.empty()) {
        StartLoadFromFile();
        break;
      }
      // There was no configured model path. Switch the state to IDLE and
      // fall through to consider the URL.
      state_ = LoaderState::IDLE;
      FALLTHROUGH;
    case LoaderState::IDLE:
      if (model_url_.is_valid()) {
        StartLoadFromURL();
        break;
      }
      // There was no configured model URL. Switch the state to FINISHED and
      // fall through.
      state_ = LoaderState::FINISHED;
      FALLTHROUGH;
    case LoaderState::FINISHED:
    case LoaderState::LOADING_FROM_FILE:
    case LoaderState::LOADING_FROM_URL:
      // Nothing to do.
      break;
  }
}

void RankerModelLoaderImpl::StartLoadFromFile() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, LoaderState::NOT_STARTED);
  DCHECK(!model_path_.empty());
  state_ = LoaderState::LOADING_FROM_FILE;
  load_start_time_ = base::TimeTicks::Now();
  base::PostTaskAndReplyWithResult(
      background_task_runner_.get(), FROM_HERE,
      base::BindOnce(&LoadFromFile, model_path_),
      base::BindOnce(&RankerModelLoaderImpl::OnFileLoaded,
                     weak_ptr_factory_.GetWeakPtr()));
}

void RankerModelLoaderImpl::OnFileLoaded(const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, LoaderState::LOADING_FROM_FILE);

  // Record the duration of the download.
  RecordTimerHistogram(uma_prefix_ + kReadTimerHistogram,
                       base::TimeTicks::Now() - load_start_time_);

  // Empty data means |model_path| wasn't successfully read. Otherwise,
  // parse and validate the model.
  std::unique_ptr<RankerModel> model;
  if (data.empty()) {
    ReportModelStatus(RankerModelStatus::LOAD_FROM_CACHE_FAILED);
  } else {
    model = CreateAndValidateModel(data);
  }

  // If |model| is nullptr, then data is empty or the parse failed. Transition
  // to IDLE, from which URL download can be attempted.
  if (!model) {
    state_ = LoaderState::IDLE;
  } else {
    // The model is valid. The client is willing/able to use it. Keep track
    // of where it originated and whether or not is has expired.
    std::string url_spec = model->GetSourceURL();
    bool is_expired = model->IsExpired();
    bool is_finished = url_spec == model_url_.spec() && !is_expired;

    DVLOG(2) << (is_expired ? "Expired m" : "M") << "odel in '"
             << model_path_.value() << "' was originally downloaded from '"
             << url_spec << "'.";

    // If the cached model came from currently configured |model_url_| and has
    // not expired, transition to FINISHED, as there is no need for a model
    // download; otherwise, transition to IDLE.
    state_ = is_finished ? LoaderState::FINISHED : LoaderState::IDLE;

    // Transfer the model to the client.
    on_model_available_cb_.Run(std::move(model));
  }

  // Notify the state machine. This will immediately kick off a download if
  // one is required, instead of waiting for the next organic detection of
  // ranker activity.
  NotifyOfRankerActivity();
}

void RankerModelLoaderImpl::StartLoadFromURL() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, LoaderState::IDLE);
  DCHECK(model_url_.is_valid());

  // Do nothing if the download attempts should be throttled.
  if (base::TimeTicks::Now() < next_earliest_download_time_) {
    DVLOG(2) << "Last download attempt was too recent.";
    ReportModelStatus(RankerModelStatus::DOWNLOAD_THROTTLED);
    return;
  }

  // Kick off the next download attempt and reset the time of the next earliest
  // allowable download attempt.
  DVLOG(2) << "Downloading model from: " << model_url_;
  state_ = LoaderState::LOADING_FROM_URL;
  load_start_time_ = base::TimeTicks::Now();
  next_earliest_download_time_ =
      load_start_time_ + base::TimeDelta::FromMinutes(kMinRetryDelayMins);
  bool request_started =
      url_fetcher_->Request(model_url_,
                            base::BindOnce(&RankerModelLoaderImpl::OnURLFetched,
                                           weak_ptr_factory_.GetWeakPtr()),
                            url_loader_factory_.get());

  // |url_fetcher_| maintains a request retry counter. If all allowed attempts
  // have already been exhausted, then the loader is finished and has abandoned
  // loading the model.
  if (!request_started) {
    DVLOG(2) << "Model download abandoned.";
    ReportModelStatus(RankerModelStatus::MODEL_LOADING_ABANDONED);
    state_ = LoaderState::FINISHED;
  }
}

void RankerModelLoaderImpl::OnURLFetched(bool success,
                                         const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_EQ(state_, LoaderState::LOADING_FROM_URL);

  // Record the duration of the download.
  RecordTimerHistogram(uma_prefix_ + kDownloadTimerHistogram,
                       base::TimeTicks::Now() - load_start_time_);

  // On request failure, transition back to IDLE. The loader will retry, or
  // enforce the max download attempts, later.
  if (!success || data.empty()) {
    DVLOG(2) << "Download from '" << model_url_ << "'' failed.";
    ReportModelStatus(RankerModelStatus::DOWNLOAD_FAILED);
    state_ = LoaderState::IDLE;
    return;
  }

  // Attempt to loads the model. If this fails, transition back to IDLE. The
  // loader will retry, or enfore the max download attempts, later.
  auto model = CreateAndValidateModel(data);
  if (!model) {
    DVLOG(2) << "Model from '" << model_url_ << "'' not valid.";
    state_ = LoaderState::IDLE;
    return;
  }

  // The model is valid. Update the metadata to track the source URL and
  // download timestamp.
  auto* metadata = model->mutable_proto()->mutable_metadata();
  metadata->set_source(model_url_.spec());
  metadata->set_last_modified_sec(
      (base::Time::Now() - base::Time()).InSeconds());

  // Cache the model to model_path_, in the background.
  if (!model_path_.empty()) {
    background_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&SaveToFile, model_url_, model_path_,
                                  model->SerializeAsString(), uma_prefix_));
  }

  // The loader is finished. Transfer the model to the client.
  state_ = LoaderState::FINISHED;
  on_model_available_cb_.Run(std::move(model));
}

std::unique_ptr<RankerModel> RankerModelLoaderImpl::CreateAndValidateModel(
    const std::string& data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  MyScopedHistogramTimer timer(uma_prefix_ + kParsetimerHistogram);
  auto model = RankerModel::FromString(data);
  if (ReportModelStatus(model ? validate_model_cb_.Run(*model)
                              : RankerModelStatus::PARSE_FAILED) !=
      RankerModelStatus::OK) {
    return nullptr;
  }
  return model;
}

RankerModelStatus RankerModelLoaderImpl::ReportModelStatus(
    RankerModelStatus model_status) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::HistogramBase* histogram = base::LinearHistogram::FactoryGet(
      uma_prefix_ + kModelStatusHistogram, 1,
      static_cast<int>(RankerModelStatus::MAX),
      static_cast<int>(RankerModelStatus::MAX) + 1,
      base::HistogramBase::kUmaTargetedHistogramFlag);
  if (histogram)
    histogram->Add(static_cast<int>(model_status));
  return model_status;
}

}  // namespace assist_ranker
