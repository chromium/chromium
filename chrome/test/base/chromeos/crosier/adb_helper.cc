// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/chromeos/crosier/adb_helper.h"

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/task/single_thread_task_runner.h"
#include "base/threading/thread_restrictions.h"
#include "base/time/time.h"
#include "chrome/test/base/chromeos/crosier/helper/test_sudo_helper_client.h"

namespace {

// Android vendtor key to authorize adb connection.
constexpr char kArcKey[] = R"(-----BEGIN PRIVATE KEY-----
MIIEvAIBADANBgkqhkiG9w0BAQEFAASCBKYwggSiAgEAAoIBAQCnHNzujonYRLoI
F2pyJX1SSrqmiT/3rTRCP1X0pj1V/sPGwgvIr+3QjZehLUGRQL0wneBNXd6EVrST
drO4cOPwSxRJjCf+/PtS1nwkz+o/BGn5yhNppdSro7aPoQxEVM8qLtN5Ke9tx/zE
ggxpF8D3XBC6Los9lAkyesZI6xqXESeofOYu3Hndzfbz8rAjC0X+p6Sx561Bt1dn
T7k2cP0mwWfITjW8tAhzmKgL4tGcgmoLhMHl9JgScFBhW2Nd0QAR4ACyVvryJ/Xa
2L6T2YpUjqWEDbiJNEApFb+m+smIbyGz0H/Kj9znoRs84z3/8rfyNQOyf7oqBpr2
52XG4totAgMBAAECggEARisKYWicXKDO9CLQ4Uj4jBswsEilAVxKux5Y+zbqPjeR
AN3tkMC+PHmXl2enRlRGnClOS24ExtCZVenboLBWJUmBJTiieqDC7o985QAgPYGe
9fFxoUSuPbuqJjjbK73olq++v/tpu1Djw6dPirkcn0CbDXIJqTuFeRqwM2H0ckVl
mVGUDgATckY0HWPyTBIzwBYIQTvAYzqFHmztcUahQrfi9XqxnySI91no8X6fR323
R8WQ44atLWO5TPCu5JEHCwuTzsGEG7dEEtRQUxAsH11QC7S53tqf10u40aT3bXUh
XV62ol9Zk7h3UrrlT1h1Ae+EtgIbhwv23poBEHpRQQKBgQDeUJwLfWQj0xHO+Jgl
gbMCfiPYvjJ9yVcW4ET4UYnO6A9bf0aHOYdDcumScWHrA1bJEFZ/cqRvqUZsbSsB
+thxa7gjdpZzBeSzd7M+Ygrodi6KM/ojSQMsen/EbRFerZBvsXimtRb88NxTBIW1
RXRPLRhHt+VYEF/wOVkNZ5c2eQKBgQDAbwNkkVFTD8yQJFxZZgr1F/g/nR2IC1Yb
ylusFztLG998olxUKcWGGMoF7JjlM6pY3nt8qJFKek9bRJqyWSqS4/pKR7QTU4Nl
a+gECuD3f28qGFgmay+B7Fyi9xmBAsGINyVxvGyKH95y3QICw1V0Q8uuNwJW2feo
3+UD2/rkVQKBgFloh+ljC4QQ3gekGOR0rf6hpl8D1yCZecn8diB8AnVRBOQiYsX9
j/XDYEaCDQRMOnnwdSkafSFfLbBrkzFfpe6viMXSap1l0F2RFWhQW9yzsvHoB4Br
W7hmp73is2qlWQJimIhLKiyd3a4RkoidnzI8i5hEUBtDsqHVHohykfDZAoGABNhG
q5eFBqRVMCPaN138VKNf2qon/i7a4iQ8Hp8PHRr8i3TDAlNy56dkHrYQO2ULmuUv
Erpjvg5KRS/6/RaFneEjgg9AF2R44GrREpj7hP+uWs72GTGFpq2+v1OdTsQ0/yr0
RGLMEMYwoY+y50Lnud+jFyXHZ0xhkdzhNTGqpWkCgYBigHVt/p8uKlTqhlSl6QXw
1AyaV/TmfDjzWaNjmnE+DxQfXzPi9G+cXONdwD0AlRM1NnBRN+smh2B4RBeU515d
x5RpTRFgzayt0I4Rt6QewKmAER3FbbPzaww2pkfH1zr4GJrKQuceWzxUf46K38xl
yee+dcuGhs9IGBOEEF7lFA==
-----END PRIVATE KEY-----)";

// Install the vendor key in the given path and return the full path to the key
// file.
base::FilePath InstallVendorKey(const base::FilePath& path) {
  base::FilePath key_file = path.Append("crosier.adb_key");
  base::WriteFile(key_file, kArcKey);

  constexpr char command_template[] = R"(
    KEY_DIR="%s"
    chown -R root.root "$KEY_DIR"
    chmod 0755 "$KEY_DIR"
    chmod 0600 "%s"
  )";
  auto result = TestSudoHelperClient().RunCommand(base::StringPrintf(
      command_template, path.value().c_str(), key_file.value().c_str()));
  CHECK_EQ(result.return_code, 0);

  return key_file;
}

// Removes the vendor key files.
void RemoveVendorKey(base::ScopedTempDir dir) {
  if (!dir.IsValid()) {
    return;
  }

  constexpr char command_template[] = R"(
     rm -rf "%s"
  )";
  auto result = TestSudoHelperClient().RunCommand(
      base::StringPrintf(command_template, dir.Take().value().c_str()));
  CHECK_EQ(result.return_code, 0);
}

void KillServer() {
  // `adb kill-server` is not reliable (crbug.com/855325).
  // Not using `killall` since it can wait for orphan adb processes indefinitely
  // (b/137797801).
  //
  // Use `kill -9` directly.
  TestSudoHelperClient().RunCommand(R"(
     while pgrep adb; do
       kill -9 $(pgrep adb)
       sleep 0.1
     done
  )");
}

void GiveItSomeTime(base::TimeDelta t) {
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(), t);
  run_loop.Run();
}

}  // namespace

AdbHelper::AdbHelper() = default;

AdbHelper::~AdbHelper() {
  if (!initialized_) {
    return;
  }

  KillServer();

  base::ScopedAllowBlockingForTesting allow_blocking;
  RemoveVendorKey(std::move(vendor_key_dir_));
}

void AdbHelper::Intialize() {
  initialized_ = true;

  KillServer();

  base::ScopedAllowBlockingForTesting allow_blocking;
  CHECK(vendor_key_dir_.CreateUniqueTempDir())
      << "Failed to create temp dir to hold vendor key.";
  vendor_key_file_ = InstallVendorKey(vendor_key_dir_.GetPath());
  CHECK(!vendor_key_file_.empty());

  constexpr char command_template[] = R"(
      ADB_VENDOR_KEYS=%s adb start-server
  )";
  auto result = TestSudoHelperClient().RunCommand(
      base::StringPrintf(command_template, vendor_key_file_.value().c_str()));
  CHECK_EQ(result.return_code, 0);

  WaitForDevice();
  CHECK(!serial_.empty());
}

bool AdbHelper::InstallApk(const base::FilePath& apk_path) {
  {
    base::ScopedAllowBlockingForTesting allow_blocking;
    if (!base::PathExists(apk_path)) {
      LOG(ERROR) << "Apk file does not exist: " << apk_path;
      return false;
    }
  }

  // Disable apk check.
  auto result = TestSudoHelperClient().RunCommand(R"(
    adb shell settings put global verifier_verify_adb_installs 0
  )");
  CHECK_EQ(result.return_code, 0);

  // Install the apk.
  const bool success =
      Command(base::StringPrintf(R"(install "%s")", apk_path.value().c_str()));
  if (success) {
    return true;
  }

  // Dump logcat to debug the installation failure. The output of `logcat` would
  // be in sudo helper command output so not dumping it again here.
  Command("logcat -d");
  return false;
}

bool AdbHelper::Command(std::string_view command) {
  auto result = TestSudoHelperClient().RunCommand(
      base::StrCat({"ADB_VENDOR_KEYS=", vendor_key_file_.value(), " adb -s ",
                    serial_, " ", command}));
  return result.return_code == 0;
}

void AdbHelper::WaitForDevice() {
  constexpr char kEmulatorPrefix[] = "emulator-";
  constexpr char kStateDevice[] = "device";

  constexpr char command_template[] = R"(
    ADB_VENDOR_KEYS=%s adb devices
  )";
  const std::string adb_devices =
      base::StringPrintf(command_template, vendor_key_file_.value().c_str());

  do {
    GiveItSomeTime(base::Milliseconds(500));

    // List all devices.
    auto result = TestSudoHelperClient().RunCommand(adb_devices);
    CHECK_EQ(result.return_code, 0);

    // Search for the first emulator device with "device" state.
    const auto lines = base::SplitString(
        result.output, "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    for (const auto& line : lines) {
      const auto fields = base::SplitString(line, " \t", base::TRIM_WHITESPACE,
                                            base::SPLIT_WANT_NONEMPTY);
      if (fields.size() != 2) {
        continue;
      }

      if (base::StartsWith(fields[0], kEmulatorPrefix) &&
          fields[1] == kStateDevice) {
        serial_ = fields[0];
        break;
      }
    }
  } while (serial_.empty());
}
