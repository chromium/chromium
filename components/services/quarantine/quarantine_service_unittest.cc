// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "components/services/quarantine/public/mojom/quarantine.mojom.h"
#include "components/services/quarantine/quarantine_impl.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace quarantine {

const char kTestData[] = "It's okay to have a trailing nul.";
const char kInternetURL[] = "http://example.com/some-url";
const char kInternetReferrerURL[] = "http://example.com/some-other-url";

class QuarantineServiceTest : public testing::Test {
 public:
  QuarantineServiceTest() = default;

  QuarantineServiceTest(const QuarantineServiceTest&) = delete;
  QuarantineServiceTest& operator=(const QuarantineServiceTest&) = delete;

  ~QuarantineServiceTest() override = default;

  void OnFileQuarantined(const base::FilePath& test_file,
                         base::OnceClosure quit_closure,
                         mojom::QuarantineFileResult result) {
    base::DeleteFile(test_file);
    result_ = result;
    std::move(quit_closure).Run();
  }

 protected:
  base::test::TaskEnvironment task_environment_;
  mojo::Remote<mojom::Quarantine> quarantine_;
  mojom::QuarantineFileResult result_;

 private:
  QuarantineImpl service_{quarantine_.BindNewPipeAndPassReceiver()};
};

TEST_F(QuarantineServiceTest, QuarantineFile) {
  base::ScopedTempDir test_dir;
  ASSERT_TRUE(test_dir.CreateUniqueTempDir());

  base::FilePath test_file = test_dir.GetPath().AppendASCII("foo.class");
  ASSERT_TRUE(base::WriteFile(test_file, kTestData));

  base::RunLoop run_loop;
  quarantine_->QuarantineFile(
      test_file, GURL(kInternetURL), GURL(kInternetReferrerURL),
      /*request_initiator=*/std::nullopt, std::string(),
      base::BindOnce(&QuarantineServiceTest::OnFileQuarantined,
                     base::Unretained(this), test_file,
                     run_loop.QuitClosure()));
  run_loop.Run();
  EXPECT_EQ(result_, mojom::QuarantineFileResult::OK);
}

}  // namespace quarantine
