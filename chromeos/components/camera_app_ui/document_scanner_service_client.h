// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_CAMERA_APP_UI_DOCUMENT_SCANNER_SERVICE_CLIENT_H_
#define CHROMEOS_COMPONENTS_CAMERA_APP_UI_DOCUMENT_SCANNER_SERVICE_CLIENT_H_

#include <memory>

#include "base/callback.h"
#include "base/sequence_checker.h"
#include "base/sequenced_task_runner.h"
#include "chromeos/services/machine_learning/public/mojom/document_scanner.mojom.h"
#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/gfx/geometry/point_f.h"

namespace chromeos {

// Client for communicating to the CrOS Document Scanner Service.
class DocumentScannerServiceClient {
 public:
  using DetectCornersCallback =
      base::OnceCallback<void(bool success,
                              const std::vector<gfx::PointF>& results)>;
  using DoPostProcessingCallback = base::OnceCallback<
      void(bool success, const std::vector<uint8_t>& processed_jpeg_image)>;

  static bool IsSupported();

  static std::unique_ptr<DocumentScannerServiceClient> Create();

  ~DocumentScannerServiceClient();

  bool IsLoaded();

  void DetectCornersFromNV12Image(base::ReadOnlySharedMemoryRegion nv12_image,
                                  DetectCornersCallback callback);

  void DetectCornersFromJPEGImage(base::ReadOnlySharedMemoryRegion jpeg_image,
                                  DetectCornersCallback callback);

  void DoPostProcessing(base::ReadOnlySharedMemoryRegion jpeg_image,
                        const std::vector<gfx::PointF>& corners,
                        DoPostProcessingCallback callback);

 protected:
  explicit DocumentScannerServiceClient();

 private:
  void OnInitialized(chromeos::machine_learning::mojom::LoadModelResult result);

  bool document_scanner_loaded_ = false;

  mojo::Remote<chromeos::machine_learning::mojom::MachineLearningService>
      ml_service_;

  mojo::Remote<chromeos::machine_learning::mojom::DocumentScanner>
      document_scanner_;
};

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_CAMERA_APP_UI_DOCUMENT_SCANNER_SERVICE_CLIENT_H_
