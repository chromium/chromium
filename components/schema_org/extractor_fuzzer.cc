// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/run_loop.h"
#include "base/task/single_thread_task_executor.h"
#include "base/test/bind_test_util.h"
#include "components/schema_org/common/improved_metadata.mojom.h"
#include "components/schema_org/extractor.h"
#include "components/schema_org/schema_org_entity_names.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/libfuzzer/libfuzzer_exports.h"

void FuzzExtractor(const uint8_t* data, size_t size) {
  data_decoder::test::InProcessDataDecoder data_decoder;
  std::string fuzz_input =
      std::string(reinterpret_cast<const char*>(data), size);

  base::RunLoop run_loop;

  schema_org::Extractor extractor({schema_org::entity::kCompleteDataFeed});
  extractor.Extract(fuzz_input,
                    base::BindLambdaForTesting(
                        [&](schema_org::improved::mojom::EntityPtr entity) {
                          run_loop.Quit();
                        }));

  run_loop.Run();
}

class Env {
 public:
  scoped_refptr<base::SingleThreadTaskRunner> task_runner() {
    return executor_.task_runner();
  }

 private:
  base::SingleThreadTaskExecutor executor_;
};

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  if (size == 0)
    return 0;

  static Env env;
  env.task_runner()->PostTask(FROM_HERE,
                              base::BindOnce(&FuzzExtractor, data, size));

  return 0;
}
