// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_HINTS_PROCESSING_UTIL_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_HINTS_PROCESSING_UTIL_H_

#include <string>

#include "components/optimization_guide/proto/hints.pb.h"
#include "net/nqe/effective_connection_type.h"

class GURL;

namespace optimization_guide {
class StoreUpdateData;

// Returns the string representation of the optimization type.
std::string GetStringNameForOptimizationType(
    proto::OptimizationType optimization_type);

// Returns whether |optimization| is disabled subject to it being part of
// an optimization hint experiment. |optimization| could be disabled either
// because: it is only to be used with a named optimization experiment; or it
// is not to be used with a named excluded experiment. One experiment name
// may be configured for the client with the experiment_name parameter to the
// kOptimizationHintsExperiments feature.
bool IsDisabledPerOptimizationHintExperiment(
    const proto::Optimization& optimization);

// Returns the matching PageHint for |gurl| if found in |hint|.
const proto::PageHint* FindPageHintForURL(const GURL& gurl,
                                          const proto::Hint* hint);

// The host is hashed and returned as a string because base::DictionaryValue
// only accepts strings as keys. Note, some hash collisions could occur on
// hosts. For querying the blacklist, collisions are acceptable as they would
// only block additional hosts. For updating the blacklist, a collision would
// enable a site that should remain on the blacklist. However, the likelihood
// of a collision for the number of hosts allowed in the blacklist is
// practically zero.
std::string HashHostForDictionary(const std::string& host);

// Verifies and processes |hints| and places the ones it supports into
// |update_data|.
//
// Returns true if there was at least one hint moved into |update_data|.
bool ProcessHints(google::protobuf::RepeatedPtrField<proto::Hint>* hints,
                  StoreUpdateData* update_data);

// Converts |proto_ect| into a net::EffectiveConnectionType.
net::EffectiveConnectionType ConvertProtoEffectiveConnectionType(
    proto::EffectiveConnectionType proto_ect);

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_HINTS_PROCESSING_UTIL_H_
