// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_EMBEDDINGS_ML_ANSWERER_H_
#define COMPONENTS_HISTORY_EMBEDDINGS_ML_ANSWERER_H_

#include "components/history_embeddings/answerer.h"
#include "components/history_embeddings/mock_answerer.h"

namespace history_embeddings {

// TODO: b/343237382 - Integrate History Question Answerer ML model
class MlAnswerer : public MockAnswerer {
 public:
  MlAnswerer();
  ~MlAnswerer() override;
};

}  // namespace history_embeddings

#endif  // COMPONENTS_HISTORY_EMBEDDINGS_ML_ANSWERER_H_
