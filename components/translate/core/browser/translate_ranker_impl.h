// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_IMPL_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "components/assist_ranker/ranker_model_loader.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/translate/core/browser/translate_ranker.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "url/gurl.h"

class GURL;

namespace assist_ranker {
class RankerModel;
}  // namespace assist_ranker

namespace base {
class FilePath;
}

namespace ukm {
class UkmRecorder;
}  // namespace ukm

namespace metrics {
class TranslateEventProto;
}  // namespace metrics

namespace translate {

class TranslateMetricsLogger;

extern const char kDefaultTranslateRankerModelURL[];

// Features used to enable ranker query, enforcement and logging. Note that
// enabling enforcement implies (forces) enabling queries.
BASE_DECLARE_FEATURE(kTranslateRankerQuery);
BASE_DECLARE_FEATURE(kTranslateRankerEnforcement);
BASE_DECLARE_FEATURE(kTranslateRankerPreviousLanguageMatchesOverride);

struct TranslateRankerFeatures {
  TranslateRankerFeatures();

  TranslateRankerFeatures(int accepted,
                          int denied,
                          int ignored,
                          const std::string& src,
                          const std::string& dst,
                          const std::string& cntry,
                          const std::string& locale);

  explicit TranslateRankerFeatures(const metrics::TranslateEventProto& tep);

  ~TranslateRankerFeatures();

  void WriteTo(std::ostream& stream) const;

  // Input value used to generate the features.
  int accepted_count;
  int denied_count;
  int ignored_count;
  int total_count;

  // Used for inference.
  std::string src_lang;
  std::string dst_lang;
  std::string country;
  std::string app_locale;
  double accepted_ratio;
  double denied_ratio;
  double ignored_ratio;
};

// If enabled, downloads a translate ranker model and uses it to determine
// whether the user should be given a translation prompt or not.
class TranslateRankerImpl : public TranslateRanker {
 public:
  TranslateRankerImpl(const base::FilePath& model_path,
                      const GURL& model_url,
                      ukm::UkmRecorder* ukm_recorder);

  TranslateRankerImpl(const TranslateRankerImpl&) = delete;
  TranslateRankerImpl& operator=(const TranslateRankerImpl&) = delete;

  ~TranslateRankerImpl() override;

  // Get the file path of the translate ranker model, by default with a fixed
  // name within |data_dir|.
  static base::FilePath GetModelPath(const base::FilePath& data_dir);

  // Get the URL from which the download the translate ranker model, by default
  // from Field Trial parameters.
  static GURL GetModelURL();

  // TranslateRanker...
  void EnableLogging(bool value) override;
  uint32_t GetModelVersion() const override;
  bool ShouldOfferTranslation(
      metrics::TranslateEventProto* translate_event,
      TranslateMetricsLogger* translate_metrics_logger) override;
  void FlushTranslateEvents(
      std::vector<metrics::TranslateEventProto>* events) override;
  void RecordTranslateEvent(
      int event_type,
      ukm::SourceId ukm_source_id,
      metrics::TranslateEventProto* translate_event) override;
  bool ShouldOverrideMatchesPreviousLanguageDecision(
      ukm::SourceId ukm_source_id,
      metrics::TranslateEventProto* translate_event) override;

  void OnModelAvailable(std::unique_ptr<assist_ranker::RankerModel> model);

  // Get the model decision on whether we should show the translate
  // UI or not given |translate_event|.
  bool GetModelDecision(const metrics::TranslateEventProto& translate_event);

  // Check if the ModelLoader has been initialized. Used to test ModelLoader
  // logic.
  bool CheckModelLoaderForTesting();

 private:
  void SendEventToUKM(const metrics::TranslateEventProto& translate_event,
                      ukm::SourceId ukm_source_id);

  // Caches the translate event.
  void AddTranslateEvent(const metrics::TranslateEventProto& translate_event,
                         ukm::SourceId ukm_source_id);

  // Used to log URL-keyed metrics. This pointer will outlive |this|.
  raw_ptr<ukm::UkmRecorder> ukm_recorder_;

  // Used to sanity check the threading of this ranker.
  SEQUENCE_CHECKER(sequence_checker_);

  // A helper to load the translate ranker model from disk cache or a URL.
  std::unique_ptr<assist_ranker::RankerModelLoader> model_loader_;

  // The translation ranker model.
  std::unique_ptr<assist_ranker::RankerModel> model_;

  // Tracks whether or not translate event logging is enabled.
  bool is_uma_logging_enabled_ = true;

  // Tracks whether or not translate ranker querying is enabled.
  bool is_query_enabled_ = true;

  // Tracks whether or not translate ranker enforcement is enabled. Note that
  // that also enables the code paths for translate ranker querying.
  bool is_enforcement_enabled_ = true;

  // When set to true, overrides UI suppression when previous language
  // matches current language in bubble UI.
  bool is_previous_language_matches_override_enabled_ = false;

  // Saved cache of translate event protos.
  std::vector<metrics::TranslateEventProto> event_cache_;

  base::WeakPtrFactory<TranslateRankerImpl> weak_ptr_factory_{this};
};

}  // namespace translate

std::ostream& operator<<(std::ostream& stream,
                         const translate::TranslateRankerFeatures& features);

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_RANKER_IMPL_H_
