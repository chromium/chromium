// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_
#define COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/scoped_native_library.h"
#include "components/services/screen_ai/public/mojom/screen_ai_service.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/receiver_set.h"

namespace screen_ai {

using AnnotationCallback =
    base::OnceCallback<void(mojom::ErrorType, std::vector<mojom::NodePtr>)>;

// Sends the snapshot to a local machine learning library to get annotations
// that can help in updating the accessibility tree. See more in:
// google3/chrome/chromeos/accessibility/machine_intelligence/
// chrome_screen_ai/README.md
class ScreenAIService : public mojom::ScreenAIService,
                        public mojom::ScreenAIAnnotator {
 public:
  explicit ScreenAIService(
      mojo::PendingReceiver<mojom::ScreenAIService> receiver);
  ScreenAIService(const ScreenAIService&) = delete;
  ScreenAIService& operator=(const ScreenAIService&) = delete;
  ~ScreenAIService() override;

 private:
  base::ScopedNativeLibrary library_;

  // mojom::ScreenAIAnnotator
  void Annotate(const SkBitmap& image, AnnotationCallback callback) override;

  // mojom::ScreenAIService
  void BindAnnotator(
      mojo::PendingReceiver<mojom::ScreenAIAnnotator> annotator) override;

  // Converts the serialized proto from Screen AI library to a vector of
  // screen_ai::mojom::Node.
  std::vector<mojom::Node> DecodeProto(const std::string& serialized_proto);

  typedef bool (*ScreenAIInitFunction)();
  ScreenAIInitFunction init_function_;

  typedef bool (*ScreenAIAnnotateFunction)(const unsigned char* /*png_pixels*/,
                                           int /*image_width*/,
                                           int /*image_height*/,
                                           std::string& /*annotation_text*/);
  ScreenAIAnnotateFunction annotator_function_;

  mojo::Receiver<mojom::ScreenAIService> receiver_;

  // The set of receivers used to receive messages from the renderer clients.
  mojo::ReceiverSet<mojom::ScreenAIAnnotator> screen_ai_annotators_;
};

}  // namespace screen_ai

#endif  // COMPONENTS_SERVICES_SCREEN_AI_SCREEN_AI_SERVICE_IMPL_H_
