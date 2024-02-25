// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_V8_COMPILE_HINTS_V8_COMPILE_HINTS_TAB_HELPER_H_
#define CHROME_BROWSER_V8_COMPILE_HINTS_V8_COMPILE_HINTS_TAB_HELPER_H_

#include "base/cancelable_callback.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "chrome/browser/v8_compile_hints/proto/v8_compile_hints_metadata.pb.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace optimization_guide {
class OptimizationGuideDecider;
enum class OptimizationGuideDecision;
class OptimizationMetadata;
}  // namespace optimization_guide

namespace content {
class WebContents;
}  // namespace content

// This is inside chrome/browser, because OptimizationGuide is only available
// in chrome.
namespace v8_compile_hints {

// Keep in sync with V8CompileHintsModelQuality in enums.xml.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class V8CompileHintsModelQuality {
  kNoModel = 0,
  kBadModel = 1,
  kGoodModel = 2,
  kMaxValue = kGoodModel
};

// Observes page load events, requests V8_COMPILE_HINTS data from
// OptimizationGuide, and sends it to the renderer.
//
// All methods must be called from the UI thread.
class V8CompileHintsTabHelper
    : public content::WebContentsObserver,
      public content::WebContentsUserData<V8CompileHintsTabHelper> {
 public:
  // Indirection for sending the data, used in testing.
  using SendDataToRendererFunction =
      base::RepeatingCallback<void(const proto::Model&)>;

  static const size_t kModelInt64Count = 1024;

  V8CompileHintsTabHelper(
      content::WebContents* web_contents,
      optimization_guide::OptimizationGuideDecider* optimization_guide_decider);

  V8CompileHintsTabHelper(const V8CompileHintsTabHelper&) = delete;
  V8CompileHintsTabHelper& operator=(const V8CompileHintsTabHelper&) = delete;

  ~V8CompileHintsTabHelper() override;

  static void MaybeCreateForWebContents(content::WebContents* web_contents);

  // content::WebContentsObserver implementation
  void PrimaryPageChanged(content::Page& page) override;

  void SetSendDataToRendererForTesting(
      SendDataToRendererFunction send_data_to_renderer_for_testing) {
    send_data_to_renderer_for_testing_ = send_data_to_renderer_for_testing;
  }

  static constexpr const char* kModelQualityHistogramName =
      "WebCore.Scripts.V8CrowdsourcedCompileHints.ModelQuality";

 private:
  friend class content::WebContentsUserData<V8CompileHintsTabHelper>;

  void SendDataToRenderer(const v8_compile_hints::proto::Model& model);

  // Callback for |optimization_guide_decider_|.
  void OnOptimizationGuideDecision(
      optimization_guide::OptimizationGuideDecision decision,
      const optimization_guide::OptimizationMetadata& metadata);

  // The optimization guide decider to consult for remote predictions.
  raw_ptr<optimization_guide::OptimizationGuideDecider>
      optimization_guide_decider_ = nullptr;
  raw_ptr<content::WebContents> web_contents_ = nullptr;

  // A function for bypassing sending the data to renderer in tests.
  SendDataToRendererFunction send_data_to_renderer_for_testing_;

  base::CancelableOnceCallback<void(
      optimization_guide::OptimizationGuideDecision,
      const optimization_guide::OptimizationMetadata&)>
      on_optimization_guide_decision_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace v8_compile_hints

#endif  // CHROME_BROWSER_V8_COMPILE_HINTS_V8_COMPILE_HINTS_TAB_HELPER_H_
