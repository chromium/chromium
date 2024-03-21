// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/tracing/common/etw_export_win.h"

#include <memory>

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "base/trace_event/etw_interceptor_win.h"
#include "base/trace_event/trace_event_etw_export_win.h"
#include "base/trace_event/trace_logging_minimal_win.h"
#include "third_party/perfetto/include/perfetto/tracing/core/trace_config.h"
#include "third_party/perfetto/include/perfetto/tracing/tracing.h"
#include "third_party/perfetto/protos/perfetto/config/interceptor_config.gen.h"
#include "third_party/perfetto/protos/perfetto/config/track_event/track_event_config.gen.h"

namespace tracing {
namespace {

BASE_FEATURE(kEnableEtwExports,
             "EnableEtwExports",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Used to protect the upper 16 bits reserved by winmeta.xml as they
// should not be used but older logging code and tools incorrectly used
// them.
constexpr uint64_t kCategoryKeywordMask = ~0xFFFF000000000000;

perfetto::TraceConfig CreateTraceConfigForETWKeyword(uint64_t keyword) {
  perfetto::TraceConfig config;
  auto* data_source = config.add_data_sources();
  auto* data_source_config = data_source->mutable_config();
  data_source_config->set_name("track_event");
  data_source_config->mutable_interceptor_config()->set_name("etwexport");
  perfetto::protos::gen::TrackEventConfig track_event_config =
      base::trace_event::ETWKeywordToTrackEventConfig(keyword);
  data_source_config->set_track_event_config_raw(
      track_event_config.SerializeAsString());
  return config;
}

class ETWExportController {
 public:
  ETWExportController();
  ~ETWExportController() = delete;

  // Called from the ETW EnableCallback when the state of the provider or
  // keywords has changed.
  void OnUpdate(TlmProvider::EventControlCode event);

 private:
  bool is_registration_complete_ = false;

  std::unique_ptr<perfetto::StartupTracingSession> tracing_session_;

  // The keywords that were enabled last time the callback was made.
  uint64_t etw_match_any_keyword_ = 0;

  // The provider is set based on channel for MSEdge, in other Chromium
  // based browsers all channels use the same GUID/provider.
  std::unique_ptr<TlmProvider> etw_provider_;
};

ETWExportController::ETWExportController() {
  // Construct the ETW provider. If construction fails then the event logging
  // calls will fail. We're passing a callback function as part of registration.
  // This allows us to detect changes to enable/disable/keyword changes.
  etw_provider_ = std::make_unique<TlmProvider>(
      "Google.Chrome", base::trace_event::Chrome_GUID,
      base::BindRepeating(&ETWExportController::OnUpdate,
                          base::Unretained(this)));
  base::trace_event::ETWInterceptor::Register(etw_provider_.get());
  is_registration_complete_ = true;

  if (etw_provider_->IsEnabled()) {
    OnUpdate(TlmProvider::EventControlCode::kEnableProvider);
  }
}

void ETWExportController::OnUpdate(TlmProvider::EventControlCode event) {
  if (!is_registration_complete_) {
    return;
  }
  if (event == TlmProvider::EventControlCode::kDisableProvider) {
    if (tracing_session_) {
      tracing_session_->Abort();
      tracing_session_ = nullptr;
    }
    etw_match_any_keyword_ = 0;
    return;
  }
  if (event == TlmProvider::EventControlCode::kEnableProvider) {
    if (etw_match_any_keyword_ ==
        (etw_provider_->keyword_any() & kCategoryKeywordMask)) {
      return;
    }
    etw_match_any_keyword_ =
        etw_provider_->keyword_any() & kCategoryKeywordMask;
    if (tracing_session_) {
      tracing_session_->Abort();
      tracing_session_ = nullptr;
    }

    // ETW exporter creates a (local) startup session for the current
    // process that's never adopted and doesn't timeout. Since every
    // process enables export, an independent session is created in each
    // process.
    perfetto::Tracing::SetupStartupTracingOpts opts;
    opts.timeout_ms = 0;
    opts.backend = perfetto::kCustomBackend;

    perfetto::TraceConfig config =
        CreateTraceConfigForETWKeyword(etw_match_any_keyword_);
    tracing_session_ = perfetto::Tracing::SetupStartupTracing(config, opts);
  }
}

}  // namespace

// static
void EnableETWExport() {
  if (!base::FeatureList::IsEnabled(kEnableEtwExports)) {
    return;
  }
  static base::NoDestructor<ETWExportController> instance{};
}

}  // namespace tracing
