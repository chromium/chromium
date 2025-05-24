// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_ELEVATION_SERVICE_ELEVATOR_H_
#define CHROME_ELEVATION_SERVICE_ELEVATOR_H_

#include <windows.h>

#include <wrl/implements.h>

#include <string>

#include "base/gtest_prod_util.h"
#include "chrome/elevation_service/elevation_service_idl.h"

namespace elevation_service {

// These flags can modify the EncryptData operation. They are packed inside the
// `protection_level` argument. Access these via the `EncryptAppBoundString` API
// in chrome.
struct EncryptFlags {
  // Specify that the Encrypt operation should always use the latest key.
  bool use_latest_key = false;
};

inline constexpr IID kTestElevatorClsid = {
    0x416C51AC,
    0x4DEF,
    0x43CA,
    {0xE7, 0x35, 0xE7, 0x35, 0x21, 0x0A, 0xB2,
     0x57}};  // Elevator Test CLSID. {416C51AC-4DEF-43CA-96E8-E735210AB257}

namespace switches {
inline constexpr char kElevatorClsIdForTestingSwitch[] =
    "elevator-clsid-for-testing";
inline constexpr char kFakeReencryptForTestingSwitch[] =
    "elevator-fake-reencrypt-for-testing";
}  // namespace switches

namespace internal {

inline constexpr uint32_t kFlagUseLatestKey = 1 << 23;

// Update this each time a new flag is added.
inline constexpr uint32_t kMaxFlag = kFlagUseLatestKey;

// A static assert verifies the flags can always fit into 24 bits.
static_assert((kMaxFlag & 0xFFFFFF) == kMaxFlag);

constexpr uint32_t PackFlagsAndProtectionLevel(uint32_t flags,
                                               uint8_t protection_level) {
  return ((flags & 0xFFFFFF) << 8) | protection_level;
}

constexpr uint32_t ExtractFlags(uint32_t packed) {
  return (packed >> 8) & 0xFFFFFF;
}

constexpr uint8_t ExtractProtectionLevel(uint32_t packed) {
  return static_cast<uint8_t>(packed & 0xFF);
}

// Tests for these basic functions can be static asserts.
static_assert(ExtractProtectionLevel(PackFlagsAndProtectionLevel(0, 0)) == 0);
static_assert(ExtractFlags(PackFlagsAndProtectionLevel(0x123456, 0x01)) ==
              0x123456);
static_assert(ExtractFlags(PackFlagsAndProtectionLevel(0xAA123456, 0)) ==
              0x123456);
static_assert(ExtractFlags(PackFlagsAndProtectionLevel(0, 0)) == 0);
static_assert(ExtractProtectionLevel(PackFlagsAndProtectionLevel(0, 0x01)) ==
              0x01);
static_assert(
    ExtractProtectionLevel(PackFlagsAndProtectionLevel(0x1234, 0x01)) == 0x01);
static_assert(ExtractProtectionLevel(PackFlagsAndProtectionLevel(0x12345678,
                                                                 0x01)) ==
              0x01);

}  // namespace internal

class Elevator
    : public Microsoft::WRL::RuntimeClass<
          Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,
          IElevator,
          IElevatorChromium,
          IElevatorChrome,
          IElevatorChromeBeta,
          IElevatorChromeDev,
          IElevatorChromeCanary> {
 public:
  // Failure codes.
  static constexpr HRESULT kErrorCouldNotObtainCallingProcess =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA001);
  static constexpr HRESULT kErrorCouldNotGenerateValidationData =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA002);
  static constexpr HRESULT kErrorCouldNotDecryptWithUserContext =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA003);
  static constexpr HRESULT kErrorCouldNotDecryptWithSystemContext =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA004);
  static constexpr HRESULT kErrorCouldNotEncryptWithUserContext =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA005);
  static constexpr HRESULT kErrorCouldNotEncryptWithSystemContext =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA006);
  static constexpr HRESULT kValidationDidNotPass =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA007);
  static constexpr HRESULT kErrorCouldNotObtainPath =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA008);
  static constexpr HRESULT kErrorUnsupportedFilePath =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA009);
  static constexpr HRESULT kErrorUnsupportedProtectionLevel =
      MAKE_HRESULT(SEVERITY_ERROR, FACILITY_ITF, 0xA00A);

  // Success codes.
  static constexpr HRESULT kSuccessShouldReencrypt =
      MAKE_HRESULT(SEVERITY_SUCCESS, FACILITY_ITF, 0xA001);

  Elevator() = default;

  Elevator(const Elevator&) = delete;
  Elevator& operator=(const Elevator&) = delete;

  // Securely validates and runs the provided Chrome Recovery CRX elevated, by
  // first copying the CRX to a secure directory under %ProgramFiles% to
  // validate and unpack the CRX.
  IFACEMETHODIMP RunRecoveryCRXElevated(const wchar_t* crx_path,
                                        const wchar_t* browser_appid,
                                        const wchar_t* browser_version,
                                        const wchar_t* session_id,
                                        DWORD caller_proc_id,
                                        ULONG_PTR* proc_handle) override;
  IFACEMETHODIMP EncryptData(ProtectionLevel protection_level,
                             const BSTR plaintext,
                             BSTR* ciphertext,
                             DWORD* last_error) override;
  IFACEMETHODIMP DecryptData(const BSTR ciphertext,
                             BSTR* plaintext,
                             DWORD* last_error) override;

 private:
  FRIEND_TEST_ALL_PREFIXES(ElevatorTest, StringHandlingTest);
  ~Elevator() override = default;

  // Appends a string `to_append` to an existing string `base`, first the four
  // byte length then the string.
  static void AppendStringWithLength(const std::string& to_append,
                                     std::string& base);

  // Pulls a string from the start of the string, `str` is shortened to the
  // remainder of the string. `str` should have been a string previously passed
  // to AppendStringWithLength, so that it contains the four byte prefix of
  // length.
  static std::string PopFromStringFront(std::string& str);
};

}  // namespace elevation_service

#endif  // CHROME_ELEVATION_SERVICE_ELEVATOR_H_
