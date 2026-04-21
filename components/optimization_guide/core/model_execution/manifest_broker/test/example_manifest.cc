// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/manifest_broker/test/example_manifest.h"

#include "components/optimization_guide/core/model_execution/manifest_broker/test/manifest_builder.h"
#include "components/optimization_guide/proto/manifest.pb.h"

namespace optimization_guide {

proto::Manifest BuildExampleManifest() {
  return ManifestBuilder()
      // Safety model.
      .Add("safety_model_component",
           OnDemandComponent("abc12345...", "2026.1.21.1028"))
      .Add("language_detection_model_component",
           OnDemandComponent("def67890...", "2026.1.21.1028"))
      .Add("safety_model_recipe",
           SafetyModelRecipe(
               FileReference("safety_model_component", "weights.bin"),
               FileReference("language_detection_model_component", "lang.bin")))
      // Tiny model for writer.
      .Add("writer_tiny_model_component",
           OnDemandComponent("12345...", "2026.1.21.1028"))
      .Add("writer_tiny_model_recipe",
           BaseModelRecipe(
               FileReference("writer_tiny_model_component", "weights.bin"),
               BaseModelRecipeArgs{
                   proto::BaseModelRecipe::BACKEND_TYPE_CPU,
                   proto::BaseModelRecipe::PERFORMANCE_HINT_FASTEST_INFERENCE,
                   {},
                   2048}))
      .Add("writer_tiny_model_solution",
           SolutionRecipe("writer_tiny_model_recipe", kNoSafetyModel,
                          ManifestFileReference("writer_config.pb")),
           {
               DeviceUseCase{DeviceCategory::kCpu, kWriterUseCase},
               DeviceUseCase{DeviceCategory::kGpuLowTier, kWriterUseCase},
               DeviceUseCase{DeviceCategory::kGpuHighTier, kWriterUseCase},
           })
      // CPU recipes
      .Add("cpu_base_model_component",
           OnDemandComponent("5ab679....", "2025.8.21.1028"))
      .Add("cpu_base_model_recipe",
           BaseModelRecipe(
               FileReference("cpu_base_model_component", "weights.bin"),
               BaseModelRecipeArgs{
                   proto::BaseModelRecipe::BACKEND_TYPE_CPU,
                   proto::BaseModelRecipe::PERFORMANCE_HINT_FASTEST_INFERENCE,
                   {1},
                   1024}))
      .Add("proofreader_weights_component",
           OnDemandComponent("12345...", "2026.1.21.1028"))
      .Add("cpu_proofreader_lora_recipe",
           AdaptationRecipe(
               "cpu_base_model_recipe",
               FileReference("proofreader_weights_component", "weights.bin")))
      .Add(DeviceUseCase{DeviceCategory::kCpu, kLanguageModelUseCase},
           SolutionRecipe("cpu_base_model_recipe", "safety_model_recipe",
                          ManifestFileReference("language_model_config.pb")))
      .Add(DeviceUseCase{DeviceCategory::kCpu, kProofreaderUseCase},
           SolutionRecipe("cpu_proofreader_lora_recipe", kNoSafetyModel,
                          ManifestFileReference("proofreader_config.pb")))
      .Add(DeviceUseCase{DeviceCategory::kCpu, kSummarizerUseCase},
           SolutionRecipe("cpu_base_model_recipe", kNoSafetyModel,
                          ManifestFileReference("summarizer_config.pb")))
      // GPU recipes
      .Add("gpu_base_model_component",
           OnDemandComponent("5ab679....", "2025.8.8.1141"))
      // Fast GPU recipes.
      .Add("fast_gpu_base_model_recipe",
           BaseModelRecipe(
               FileReference("gpu_base_model_component", "weights.bin"),
               BaseModelRecipeArgs{
                   proto::BaseModelRecipe::BACKEND_TYPE_GPU,
                   proto::BaseModelRecipe::PERFORMANCE_HINT_FASTEST_INFERENCE,
                   {},
                   1024}))
      .Add(DeviceUseCase{DeviceCategory::kGpuLowTier, kLanguageModelUseCase},
           SolutionRecipe("fast_gpu_base_model_recipe", "safety_model_recipe",
                          ManifestFileReference("language_model_config.pb")))
      .Add(DeviceUseCase{DeviceCategory::kGpuLowTier, kProofreaderUseCase},
           SolutionRecipe("fast_gpu_base_model_recipe", kNoSafetyModel,
                          ManifestFileReference("proofreader_config.pb")))
      .Add(DeviceUseCase{DeviceCategory::kGpuLowTier, kSummarizerUseCase},
           SolutionRecipe("fast_gpu_base_model_recipe", kNoSafetyModel,
                          ManifestFileReference("summarizer_config.pb")))
      // Quality GPU recipes.
      .Add("quality_gpu_base_model_recipe",
           BaseModelRecipe(
               FileReference("gpu_base_model_component", "weights.bin"),
               BaseModelRecipeArgs{
                   proto::BaseModelRecipe::BACKEND_TYPE_GPU,
                   proto::BaseModelRecipe::PERFORMANCE_HINT_HIGHEST_QUALITY,
                   {},
                   1024}))
      .Add(
          DeviceUseCase{DeviceCategory::kGpuHighTier, kLanguageModelUseCase},
          SolutionRecipe("quality_gpu_base_model_recipe", "safety_model_recipe",
                         ManifestFileReference("language_model_config.pb")))
      .Add(DeviceUseCase{DeviceCategory::kGpuHighTier, kProofreaderUseCase},
           SolutionRecipe("quality_gpu_base_model_recipe", kNoSafetyModel,
                          ManifestFileReference("proofreader_config.pb")))
      .Add(DeviceUseCase{DeviceCategory::kGpuHighTier, kSummarizerUseCase},
           SolutionRecipe("quality_gpu_base_model_recipe", kNoSafetyModel,
                          ManifestFileReference("summarizer_config.pb")))
      .Build();
}

}  // namespace optimization_guide
