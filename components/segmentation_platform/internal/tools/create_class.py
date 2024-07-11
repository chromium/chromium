# Copyright 2022 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
"""Script to generate header cc and unittest file for a class in chromium."""

_DOCUMENTATION = r"""Usage:

To generate default model template files:
  python3 components/segmentation_platform/internal/tools/create_class.py \
      --segment_id MY_FEATURE_USER

To generate generic header and cc files:
  python3 components/segmentation_platform/internal/tools/create_class.py \
      --header src/dir/class_name.h

If any of the file already exists then prints a log and does not touch the
file, but still creates the remaining files.
"""

import argparse
import datetime
import logging
import os
import sys

_HEADER_TEMPLATE = """// Copyright {year} The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef {macro}
#define {macro}

namespace {namespace} {{

class {clas} {{
 public:
  {clas}();
  ~{clas}();

  {clas}(const {clas}&) = delete;
  {clas}& operator=(const {clas}&) = delete;

 private:
}};

}}

#endif  // {macro}
"""

_CC_TEMPLATE = """// Copyright {year} The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "{file_path}"

namespace {namespace} {{

{clas}::{clas} () = default;
{clas}::~{clas}() = default;

}}
"""

_TEST_TEMPLATE = """// Copyright {year} The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "{file_path}"

#include "testing/gtest/include/gtest/gtest.h"

namespace {namespace} {{

class {test_class} : public testing::Test {{
 public:
  {test_class}() = default;
  ~{test_class}() override = default;

  void SetUp() override {{
    Test::SetUp();
  }}

  void TearDown() override {{
    Test::TearDown();
  }}

 protected:
}};

TEST_F({test_class}, Test) {{
}}

}}
"""

_MODEL_HEADER_TEMPLATE = """// Copyright {year} The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef {macro}
#define {macro}

#include <memory>

#include "base/feature_list.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/model_provider.h"

namespace {namespace} {{

// Feature flag for enabling {clas} segment.
BASE_DECLARE_FEATURE(kSegmentationPlatform{clas});

// Model to predict whether the user belongs to {clas} segment.
class {clas} : public DefaultModelProvider {{
 public:
  static constexpr char k{clas}Key[] = "{segmentation_key}";
  static constexpr char k{clas}UmaName[] = "{clas}";

  {clas}();
  ~{clas}() override = default;

  {clas}(const {clas}&) = delete;
  {clas}& operator=(const {clas}&) = delete;

  static std::unique_ptr<Config> GetConfig();

  // ModelProvider implementation.
  std::unique_ptr<ModelConfig> GetModelConfig() override;
  void ExecuteModelWithInput(const ModelProvider::Request& inputs,
                             ExecutionCallback callback) override;
}};

}}

#endif  // {macro}
"""

_MODEL_CC_TEMPLATE = """// Copyright {year} The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "{file_path}"

#include <memory>

#include "base/task/sequenced_task_runner.h"
#include "components/segmentation_platform/internal/metadata/metadata_writer.h"
#include "components/segmentation_platform/public/config.h"
#include "components/segmentation_platform/public/proto/aggregation.pb.h"
#include "components/segmentation_platform/public/proto/model_metadata.pb.h"


namespace {namespace} {{

BASE_FEATURE(kSegmentationPlatform{clas},
             "SegmentationPlatform{clas}",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {{
using proto::SegmentId;

// Default parameters for {clas} model.
constexpr SegmentId kSegmentId = SegmentId::{segment_id};
constexpr int64_t kModelVersion = 1;
// Store 28 buckets of input data (28 days).
constexpr int64_t kSignalStorageLength = 28;
// Wait until we have 7 days of data.
constexpr int64_t kMinSignalCollectionLength = 7;
// Refresh the result every 7 days.
constexpr int64_t kResultTTLDays = 7;

// InputFeatures.

// Enum values for the Example.EnumHistogram.
constexpr std::array<int32_t, 3> kEnumValues{{
    0, 3, 4
}};

// Set UMA metrics to use as input.
// TODO: Fill in the necessary signals for prediction.
constexpr std::array<MetadataWriter::UMAFeature, 3> kUMAFeatures = {{
    // Total amount of times user action was recorded in last 14 days.
    MetadataWriter::UMAFeature::FromUserAction("UserActionName", 14),

    // Total value of all records of the histogram in last 7 days.
    MetadataWriter::UMAFeature::FromValueHistogram(
        "Example.ValueHistogram", 7, proto::Aggregation::SUM),

    // Total count of number of records of enum histogram with given values.
    MetadataWriter::UMAFeature::FromEnumHistogram(
        "Example.EnumHistogram",
        14,
        kEnumValues.data(),
        kEnumValues.size()),
}};

}}  // namespace

// static
std::unique_ptr<Config> {clas}::GetConfig() {{
  if (!base::FeatureList::IsEnabled(
          kSegmentationPlatform{clas})) {{
    return nullptr;
  }}
  auto config = std::make_unique<Config>();
  config->segmentation_key = k{clas}Key;
  config->segmentation_uma_name = k{clas}UmaName;
  config->AddSegmentId(kSegmentId,
                       std::make_unique<{clas}>());
  config->auto_execute_and_cache = false;
  return config;
}}

{clas}::{clas}()
    : DefaultModelProvider(kSegmentId) {{}}

std::unique_ptr<DefaultModelProvider::ModelConfig> {clas}::GetModelConfig() {{
  proto::SegmentationModelMetadata metadata;
  MetadataWriter writer(&metadata);
  writer.SetDefaultSegmentationMetadataConfig(
      kMinSignalCollectionLength,
      kSignalStorageLength);

  // Set output config.
  const char kNot{clas}Label[] = "Not{clas}";
  writer.AddOutputConfigForBinaryClassifier(
      0.5,
      /*positive_label=*/k{clas}UmaName,
      kNot{clas}Label);
  writer.AddPredictedResultTTLInOutputConfig(
      /*top_label_to_ttl_list=*/{{}},
      /*default_ttl=*/kResultTTLDays, proto::TimeUnit::DAY);

  // Set features.
  writer.AddUmaFeatures(kUMAFeatures.data(),
                        kUMAFeatures.size());

  return std::make_unique<ModelConfig>(std::move(metadata), kModelVersion);
}}

void {clas}::ExecuteModelWithInput(
    const ModelProvider::Request& inputs,
    ExecutionCallback callback) {{
  // Invalid inputs.
  if (inputs.size() != kUMAFeatures.size()) {{
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback), std::nullopt));
    return;
  }}

  // TODO: Update the heuristics here to return 1 when the user belongs to
  // {clas}.

  float result = 0;
  const int user_action_count = inputs[0];
  const int value_histogram_total = inputs[1];
  const int enum_hit_count = inputs[2];
  if (user_action_count && value_histogram_total && enum_hit_count)
    result = 1;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(callback), ModelProvider::Response(1, result)));
}}

}}
"""

_MODEL_TEST_TEMPLATE = """// Copyright {year} The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "{file_path}"

#include "components/segmentation_platform/embedder/default_model/default_model_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {namespace} {{

class {test_class} : public DefaultModelTestBase {{
 public:
  {test_class}() : DefaultModelTestBase(std::make_unique<{clas}>()) {{}}
  ~{test_class}() override = default;
}};

TEST_F({test_class}, InitAndFetchModel) {{
  ExpectInitAndFetchModel();
}}

TEST_F({test_class}, ExecuteModelWithInput) {{
  // TODO: Add test cases to verify if the heuristic returns the right segment.
  ExpectExecutionWithInput(/*inputs=*/{{1, 2, 3}}, /*expected_error=*/false,
                           /*expected_result=*/{{1}});
}}

}}
"""


def _GetLogger():
    """Logger for the tool."""
    logging.basicConfig(level=logging.INFO)
    logger = logging.getLogger('create_class')
    logger.setLevel(level=logging.INFO)
    return logger


def _WriteFile(path, type_str, contents):
    """Writes a file with contents to the path, if not exists."""
    if os.path.exists(path):
        _GetLogger().error('%s already exists', type_str)
        return

    _GetLogger().info('Writing %s file %s', type_str, path)
    with open(path, 'w') as f:
        f.write(contents)


def _GetClassNameFromFile(header):
    """Gets a class name from the header file name."""
    file_base = os.path.basename(header).replace('.h', '')
    class_name = ''
    for i in range(len(file_base)):
        if i == 0 or file_base[i - 1] == '_':
            class_name += file_base[i].upper()
        elif file_base[i] == '_':
            continue
        else:
            class_name += file_base[i]
    return class_name


def _GetSegmentationKeyFromFile(header):
    """Gets the segmentation key based on the header file."""
    return os.path.basename(header).replace('.h', '')


def _GetHeader(args):
    """Parses the args and returns path to the header file."""
    if args.header:
        if '.h' not in args.header:
            raise ValueError('The first argument should be a path to header')

        _GetLogger().info('Creating class for header %s', args.header)
        return args.header

    if args.segment_id:
        _PREFIXES_TO_REMOVE = [
            'OPTIMIZATION_TARGET_SEGMENTATION_', 'OPTIMIZATION_TARGET_'
        ]
        _GetLogger().info('Creating default model for %s', args.segment_id)
        model_name = args.segment_id
        for prefix in _PREFIXES_TO_REMOVE:
            print(prefix, model_name, model_name.startswith(prefix))
            if model_name.startswith(prefix):
                model_name = model_name[len(prefix):]
                break
        print(model_name)
        return (
            'components/segmentation_platform/embedder/default_model/%s.h' %
            model_name.lower())

    raise ValueError('Required either --header or --segment_id argument.')


def _CreateFilesForClass(args):
    """Creates header cc and test files for the class."""
    header_template = _HEADER_TEMPLATE
    cc_template = _CC_TEMPLATE
    test_template = _TEST_TEMPLATE
    if args.segment_id:
        header_template = _MODEL_HEADER_TEMPLATE
        cc_template = _MODEL_CC_TEMPLATE
        test_template = _MODEL_TEST_TEMPLATE

    header = _GetHeader(args)

    file_cc = header.replace('.h', '.cc')
    file_test = header.replace('.h', '_unittest.cc')

    format_args = {}
    format_args['year'] = datetime.date.today().year
    format_args['file_path'] = header
    format_args['macro'] = (
        header.replace('/', '_').replace('.', '_').upper() + '_')
    format_args['clas'] = _GetClassNameFromFile(header)
    format_args['segment_id'] = args.segment_id
    format_args['segmentation_key'] = _GetSegmentationKeyFromFile(header)
    format_args['namespace'] = args.namespace
    format_args['test_class'] = format_args['clas'] + 'Test'

    contents = header_template.format_map(format_args)
    _WriteFile(header, 'Header', contents)

    contents = cc_template.format_map(format_args)
    _WriteFile(file_cc, 'CC', contents)

    contents = test_template.format_map(format_args)
    _WriteFile(file_test, 'Test', contents)


def _CreateOptionParser():
    """Options parser for the tool."""
    parser = argparse.ArgumentParser(
        description=_DOCUMENTATION,
        formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('--header',
                        help='Path to the header file from src/',
                        default='')
    parser.add_argument('--segment_id',
                        help='The segment ID enum value',
                        default='')
    parser.add_argument('--namespace',
                        dest='namespace',
                        default='segmentation_platform')
    return parser


def main():
    parser = _CreateOptionParser()
    args = parser.parse_args()

    _CreateFilesForClass(args)


if __name__ == '__main__':
    sys.exit(main())
