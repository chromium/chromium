// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/v8_compile_hints/v8_compile_hints_tab_helper.h"

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/v8_compile_hints/v8_compile_hints_tab_helper.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "content/public/browser/navigation_handle.h"
#include "third_party/blink/public/common/features.h"

namespace v8_compile_hints {

V8CompileHintsTabHelper::~V8CompileHintsTabHelper() = default;

V8CompileHintsTabHelper::V8CompileHintsTabHelper(
    content::WebContents* web_contents,
    optimization_guide::OptimizationGuideDecider* optimization_guide_decider)
    : content::WebContentsObserver(web_contents),
      content::WebContentsUserData<V8CompileHintsTabHelper>(*web_contents),
      optimization_guide_decider_(optimization_guide_decider),
      web_contents_(web_contents) {
  CHECK(base::FeatureList::IsEnabled(blink::features::kConsumeCompileHints));
  optimization_guide_decider_->RegisterOptimizationTypes(
      {optimization_guide::proto::V8_COMPILE_HINTS});
}

void V8CompileHintsTabHelper::MaybeCreateForWebContents(
    content::WebContents* web_contents) {
  if (!base::FeatureList::IsEnabled(blink::features::kConsumeCompileHints)) {
    return;
  }

  Profile* profile =
      Profile::FromBrowserContext(web_contents->GetBrowserContext());
  optimization_guide::OptimizationGuideDecider* optimization_guide_decider =
      OptimizationGuideKeyedServiceFactory::GetForProfile(profile);
  if (!optimization_guide_decider) {
    return;
  }

  web_contents->SetUserData(&kUserDataKey,
                            std::make_unique<V8CompileHintsTabHelper>(
                                web_contents, optimization_guide_decider));
}

void V8CompileHintsTabHelper::PrimaryPageChanged(content::Page& page) {
  if (!optimization_guide_decider_) {
    return;
  }

  const GURL& current_main_frame_url =
      page.GetMainDocument().GetLastCommittedURL();
  if (!current_main_frame_url.SchemeIsHTTPOrHTTPS()) {
    return;
  }

  // TODO(chromium:1406506): Consider avoiding sending the data unnecessarily if
  // we're doing a back-forward navigation. This can be done e.g., by attaching
  // a bool flag to DocumentUserData.

  on_optimization_guide_decision_.Reset(
      base::BindOnce(&V8CompileHintsTabHelper::OnOptimizationGuideDecision,
                     base::Unretained(this)));

  optimization_guide_decider_->CanApplyOptimization(
      current_main_frame_url, optimization_guide::proto::V8_COMPILE_HINTS,
      on_optimization_guide_decision_.callback());
}

void V8CompileHintsTabHelper::OnOptimizationGuideDecision(
    optimization_guide::OptimizationGuideDecision decision,
    const optimization_guide::OptimizationMetadata& metadata) {
  auto model = metadata.ParsedMetadata<proto::Model>();
  if (!model || !model->has_sample_count() || !model->has_clear_ones() ||
      !model->has_clear_zeros() ||
      static_cast<size_t>(model->bloom_filter().size()) != kModelInt64Count) {
    base::UmaHistogramEnumeration(kModelQualityHistogramName,
                                  V8CompileHintsModelQuality::kNoModel);
    return;
  }

  // Reject models which are not good enough. The model goodness is estimated by
  // checking whether the "clear zeros" and "clear ones" counts match an
  // expected Bloom filter.
  constexpr int32_t kClearZerosMin = 32000;
  constexpr int32_t kClearOnesMin = 1000;
  if (model->clear_zeros() < kClearZerosMin ||
      model->clear_ones() < kClearOnesMin) {
    base::UmaHistogramEnumeration(kModelQualityHistogramName,
                                  V8CompileHintsModelQuality::kBadModel);
    return;
  }

  base::UmaHistogramEnumeration(kModelQualityHistogramName,
                                V8CompileHintsModelQuality::kGoodModel);

  SendDataToRenderer(*model);
}

void V8CompileHintsTabHelper::SendDataToRenderer(const proto::Model& model) {
  if (send_data_to_renderer_for_testing_) {
    send_data_to_renderer_for_testing_.Run(model);
    return;
  }

  constexpr size_t kBufferSize = kModelInt64Count * 8;
  mojo::ScopedSharedBufferHandle mojo_buffer =
      mojo::SharedBufferHandle::Create(kBufferSize);
  if (!mojo_buffer.is_valid()) {
    DVLOG(1) << "V8CompileHintsTabHelper shared memory handle is not valid";
    return;
  }
  base::WritableSharedMemoryRegion writable_region =
      mojo::UnwrapWritableSharedMemoryRegion(std::move(mojo_buffer));
  if (!writable_region.IsValid()) {
    DVLOG(1) << "V8CompileHintsTabHelper shared memory region is not valid";
    return;
  }

  base::WritableSharedMemoryMapping shared_memory_mapping =
      writable_region.Map();
  if (!shared_memory_mapping.IsValid()) {
    DVLOG(1) << "V8CompileHintsTabHelper shared memory mapping is not valid";
    return;
  }

  int64_t* memory = shared_memory_mapping.GetMemoryAs<int64_t>();

  for (size_t i = 0; i < kModelInt64Count; ++i) {
    memory[i] = model.bloom_filter().Get(i);
  }

  base::ReadOnlySharedMemoryRegion read_only_region =
      base::WritableSharedMemoryRegion::ConvertToReadOnly(
          std::move(writable_region));
  if (!read_only_region.IsValid()) {
    DVLOG(1) << "V8CompileHintsTabHelper read only shared memory region is "
                "not valid";
    return;
  }

  web_contents_->SetV8CompileHints(std::move(read_only_region));
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(V8CompileHintsTabHelper);

}  // namespace v8_compile_hints
