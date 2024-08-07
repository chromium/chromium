// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(crbug.com/356649891): Delete this file once all dependencies are
// migrated.

#ifndef COMPONENTS_SYNC_BASE_MODEL_TYPE_H_
#define COMPONENTS_SYNC_BASE_MODEL_TYPE_H_

#include <string>

#include "components/sync/base/data_type.h"

namespace syncer {

constexpr int GetNumModelTypes() {
  return GetNumDataTypes();
}

// Extract the model type from an EntitySpecifics field. ModelType is a
// local concept: the enum is not in the protocol.
inline ModelType GetModelTypeFromSpecifics(
    const sync_pb::EntitySpecifics& specifics) {
  return GetDataTypeFromSpecifics(specifics);
}

// Determine a model type from the field number of its associated
// EntitySpecifics field.  Returns UNSPECIFIED if the field number is
// not recognized.
inline ModelType GetModelTypeFromSpecificsFieldNumber(int field_number) {
  return GetDataTypeFromSpecificsFieldNumber(field_number);
}

namespace internal {

// Obtain model type from field_number and add to model_types if valid.
inline void GetModelTypeSetFromSpecificsFieldNumberListHelper(
    ModelTypeSet& model_types,
    int field_number) {
  return GetDataTypeSetFromSpecificsFieldNumberListHelper(model_types,
                                                          field_number);
}

}  // namespace internal

// Build a ModelTypeSet from a list of field numbers. Any unknown field numbers
// are ignored.
template <typename ContainerT>
inline ModelTypeSet GetModelTypeSetFromSpecificsFieldNumberList(
    const ContainerT& field_numbers) {
  return GetDataTypeSetFromSpecificsFieldNumberList(field_numbers);
}

// Return the field number of the EntitySpecifics field associated with
// a model type.
inline int GetSpecificsFieldNumberFromModelType(ModelType model_type) {
  return GetSpecificsFieldNumberFromDataType(model_type);
}

// Returns a string with application lifetime that represents the name of
// |model_type|.
inline const char* ModelTypeToDebugString(ModelType model_type) {
  return DataTypeToDebugString(model_type);
}

// Returns a string with application lifetime that is used as the histogram
// suffix for |model_type|.
inline const char* ModelTypeToHistogramSuffix(ModelType model_type) {
  return DataTypeToHistogramSuffix(model_type);
}

// Some histograms take an integer parameter that represents a model type.
// The mapping from ModelType to integer is defined here. It defines a
// completely different order than the ModelType enum itself. The mapping should
// match the SyncModelTypes mapping from integer to labels defined in enums.xml.
inline ModelTypeForHistograms ModelTypeHistogramValue(ModelType model_type) {
  return DataTypeHistogramValue(model_type);
}

// Returns for every model_type a positive unique integer that is stable over
// time and thus can be used when persisting data.
inline int ModelTypeToStableIdentifier(ModelType model_type) {
  return DataTypeToStableIdentifier(model_type);
}

// Returns the comma-separated string representation of |model_types|.
inline std::string ModelTypeSetToDebugString(ModelTypeSet model_types) {
  return DataTypeSetToDebugString(model_types);
}

}  // namespace syncer

#endif  // COMPONENTS_SYNC_BASE_MODEL_TYPE_H_
