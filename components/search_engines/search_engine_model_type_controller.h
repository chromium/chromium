// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_MODEL_TYPE_CONTROLLER_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_MODEL_TYPE_CONTROLLER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/model/model_type_store.h"

class TemplateURLService;

namespace browser_sync {

// Controller for the SEARCH_ENGINES sync data type. This class tells sync how
// to load the model for this data type, and the superclasses manage controlling
// the rest of the state of the datatype with regards to sync. This is analogous
// to SearchEngineDataTypeController but allows exercising the more modern USS
// architecture.
class SearchEngineModelTypeController : public syncer::ModelTypeController {
 public:
  // |template_url_service| represents the syncable service itself for
  // SEARCH_ENGINES. It must not be null and must outlive this object.
  // |dump_stack| allows the internal implementation (the processor) to report
  // error dumps. |store_factory| is used to instantiate a ModelTypeStore that
  // is used to persist sync [meta]data.
  SearchEngineModelTypeController(
      const base::RepeatingClosure& dump_stack,
      syncer::OnceModelTypeStoreFactory store_factory,
      TemplateURLService* template_url_service);
  ~SearchEngineModelTypeController() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(SearchEngineModelTypeController);
};

}  // namespace browser_sync

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINE_MODEL_TYPE_CONTROLLER_H_
