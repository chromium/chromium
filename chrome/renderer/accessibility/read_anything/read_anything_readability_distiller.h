// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_READABILITY_DISTILLER_H_
#define CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_READABILITY_DISTILLER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/common/read_anything/read_anything.mojom.h"
#include "chrome/renderer/accessibility/read_anything/read_anything_distiller.h"

// Concrete implementation of ReadAnythingDistiller backed by DOM-based
// Readability distillation.
//
// Readability distillation runs in the browser process. This class requests
// distillation from the browser and converts the reply into a
// DistillationResult.
class ReadabilityDistiller : public ReadAnythingDistiller {
 public:
  // Reply from the browser process: (result, title, html_content).
  using ReadabilityResultCallback = base::OnceCallback<void(
      read_anything::mojom::ReadabilityDistillationResult result,
      const std::string& title,
      const std::string& content)>;

  // Forwards a distillation request to the browser process. Supplied by the
  // controller, which owns the mojo remote.
  using RequestDistillationCallback =
      base::RepeatingCallback<void(ReadabilityResultCallback)>;

  ReadabilityDistiller(
      RequestDistillationCallback request_distillation_callback,
      DistillationCompleteCallback on_distillation_complete_callback);
  ~ReadabilityDistiller() override;
  ReadabilityDistiller(const ReadabilityDistiller&) = delete;
  ReadabilityDistiller& operator=(const ReadabilityDistiller&) = delete;

  // ReadAnythingDistiller:
  using ReadAnythingDistiller::Distill;
  void Distill(std::optional<DistillationRequest> request) override;
  void Reset() override;
  bool IsDistillationInProgress() const override;
  ReadAnythingAppModel::DistillationMethod GetDistillationMethod()
      const override;

 private:
  void OnContentReceived(
      read_anything::mojom::ReadabilityDistillationResult result,
      const std::string& title,
      const std::string& content);

  RequestDistillationCallback request_distillation_callback_;
  DistillationCompleteCallback on_distillation_complete_callback_;

  bool is_distillation_in_progress_ = false;

  base::WeakPtrFactory<ReadabilityDistiller> weak_ptr_factory_{this};
};

#endif  // CHROME_RENDERER_ACCESSIBILITY_READ_ANYTHING_READ_ANYTHING_READABILITY_DISTILLER_H_
