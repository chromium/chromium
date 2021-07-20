// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_test_utils.h"

#include "base/base64.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "components/variations/proto/client_variations.pb.h"
#include "components/variations/variations_associated_data.h"
#include "components/variations/variations_switches.h"
#include "third_party/zlib/google/compression_utils.h"

namespace variations {

const char kUncompressedBase64TestSeedData[] =
    "CigxZDI5NDY0ZmIzZDc4ZmYxNTU2ZTViNTUxYzY0NDdjYmM3NGU1ZmQwEr0BCh9VTUEtVW5pZm"
    "9ybWl0eS1UcmlhbC0xMC1QZXJjZW50GICckqUFOAFCB2RlZmF1bHRKCwoHZGVmYXVsdBABSgwK"
    "CGdyb3VwXzAxEAFKDAoIZ3JvdXBfMDIQAUoMCghncm91cF8wMxABSgwKCGdyb3VwXzA0EAFKDA"
    "oIZ3JvdXBfMDUQAUoMCghncm91cF8wNhABSgwKCGdyb3VwXzA3EAFKDAoIZ3JvdXBfMDgQAUoM"
    "Cghncm91cF8wORAB";

const char kCompressedBase64TestSeedData[] =
    "H4sIAAAAAAAAAOPSMEwxsjQxM0lLMk4xt0hLMzQ1NUs1TTI1NUw2MzExT05KNjdJNU1LMRDay8"
    "glH+rrqBual5mWX5SbWVKpG1KUmZija2igG5BalJyaVyLRMGfSUlYLRif2lNS0xNKcEi9uLhhT"
    "gNGLh4sjvSi/tCDewBCFZ4TCM0bhmaDwTFF4Zig8cxSeBQrPUoARAEVeJPrqAAAA";

const char kBase64TestSeedSignature[] =
    "MEQCIDD1IVxjzWYncun+9IGzqYjZvqxxujQEayJULTlbTGA/AiAr0oVmEgVUQZBYq5VLOSvy96"
    "JkMYgzTkHPwbv7K/CmgA==";

const char kTestSeedStudyName[] = "UMA-Uniformity-Trial-10-Percent";

void DisableTestingConfig() {
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kDisableFieldTrialTestingConfig);
}

bool ExtractVariationIds(const std::string& variations,
                         std::set<VariationID>* variation_ids,
                         std::set<VariationID>* trigger_ids) {
  std::string serialized_proto;
  if (!base::Base64Decode(variations, &serialized_proto))
    return false;
  ClientVariations proto;
  if (!proto.ParseFromString(serialized_proto))
    return false;
  for (int i = 0; i < proto.variation_id_size(); ++i)
    variation_ids->insert(proto.variation_id(i));
  for (int i = 0; i < proto.trigger_variation_id_size(); ++i)
    trigger_ids->insert(proto.trigger_variation_id(i));
  return true;
}

scoped_refptr<base::FieldTrial> CreateTrialAndAssociateId(
    const std::string& trial_name,
    const std::string& default_group_name,
    IDCollectionKey key,
    VariationID id) {
  AssociateGoogleVariationID(key, trial_name, default_group_name, id);
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::CreateFieldTrial(trial_name, default_group_name));
  DCHECK(trial);

  if (trial) {
    // Ensure the trial is registered under the correct key so we can look it
    // up.
    trial->group();
  }

  return trial;
}

}  // namespace variations
