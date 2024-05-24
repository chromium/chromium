// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_MAHI_AX_TREE_EXTRACTOR_H_
#define CHROMEOS_COMPONENTS_MAHI_AX_TREE_EXTRACTOR_H_

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/components/mahi/public/mojom/content_extraction.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "ui/accessibility/ax_tree.h"

namespace mahi {

using ExtractContentCallback =
    mojom::ContentExtractionService::ExtractContentCallback;
using GetContentSizeCallback =
    mojom::ContentExtractionService::GetContentSizeCallback;
using OnAxTreeDistilledCallback =
    base::OnceCallback<void(const std::vector<ui::AXNodeID>&)>;

// `AXTreeExtractor` is a class that distills an AXTreeUpdate, and then either
// extracts the contents or gets the size of the contents.
// If `kScreen2x` is chosen, the distillation is performed by the Screen2x model
// in another utility process.
// If `kAlgorithm` is chosen, the distillation is performed by rule based
// algorithm defined in this file.
class AXTreeExtractor {
 public:
  AXTreeExtractor();
  AXTreeExtractor(const AXTreeExtractor&) = delete;
  AXTreeExtractor& operator=(const AXTreeExtractor&) = delete;
  ~AXTreeExtractor();

  void OnScreen2xReady(
      mojo::PendingRemote<screen_ai::mojom::Screen2xMainContentExtractor>
          screen2x_content_extractor);

  void ExtractContent(mojom::ExtractionRequestPtr extraction_request,
                      ExtractContentCallback callback);

  void GetContentSize(mojom::ExtractionRequestPtr extraction_request,
                      GetContentSizeCallback callback);

 private:
  void ExtractContentFromSnapshot(
      mojom::ExtractionRequestPtr extraction_request,
      ExtractContentCallback callback);

  void ExtractContentFromAXTreeUpdates(
      mojom::ExtractionRequestPtr extraction_request,
      ExtractContentCallback callback);

  void DistillViaAlgorithm(const ui::AXTree* tree,
                           std::vector<ui::AXNodeID>* content_node_ids);

  void OnGetScreen2xResult(
      std::vector<ui::AXNodeID> content_node_ids_algorithm,
      OnAxTreeDistilledCallback on_ax_tree_distilled_callback,
      const std::vector<ui::AXNodeID>& content_node_ids_screen2x);

  void OnDistilledForContentExtraction(
      std::unique_ptr<ui::AXTree> tree,
      ExtractContentCallback callback,
      mojom::ResponseStatus status,
      const std::vector<ui::AXNodeID>& content_node_ids);

  void OnDistilledForContentSize(
      std::unique_ptr<ui::AXTree> tree,
      GetContentSizeCallback callback,
      mojom::ResponseStatus status,
      const std::vector<ui::AXNodeID>& content_node_ids);

  // The remote of the Screen2x main content extractor. The receiver lives in
  // another utility process.
  mojo::Remote<screen_ai::mojom::Screen2xMainContentExtractor>
      screen2x_main_content_extractor_;
  base::WeakPtrFactory<AXTreeExtractor> weak_ptr_factory_{this};
};

}  // namespace mahi

#endif  // CHROMEOS_COMPONENTS_MAHI_AX_TREE_EXTRACTOR_H_
