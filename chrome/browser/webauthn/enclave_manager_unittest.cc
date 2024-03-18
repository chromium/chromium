// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/enclave_manager.h"

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/json/json_reader.h"
#include "base/path_service.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/webauthn/fake_security_domain_service.h"
#include "chrome/browser/webauthn/proto/enclave_local_state.pb.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/protocol/webauthn_credential_specifics.pb.h"
#include "components/trusted_vault/command_line_switches.h"
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/trusted_vault_server_constants.h"
#include "crypto/scoped_fake_user_verifying_key_provider.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "device/fido/ctap_get_assertion_request.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/enclave/enclave_authenticator.h"
#include "device/fido/enclave/types.h"
#include "device/fido/test_callback_receiver.h"
#include "net/base/port_util.h"
#include "net/http/http_status_code.h"
#include "services/network/network_service.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/network_context.mojom.h"
#include "services/network/test/fake_test_cert_verifier_params_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

// These tests are also disabled under MSAN. The enclave subprocess is written
// in Rust and FFI from Rust to C++ doesn't work in Chromium at this time
// (crbug.com/1369167).
#if !defined(MEMORY_SANITIZER)

namespace enclave = device::enclave;
using NoArgCallback = device::test::TestCallbackReceiver<>;
using BoolCallback = device::test::TestCallbackReceiver<bool>;

namespace {

constexpr int32_t kSecretVersion = 417;

constexpr std::array<uint8_t, 32> kTestKey = {
    0xc4, 0xdf, 0xa4, 0xed, 0xfc, 0xf9, 0x7c, 0xc0, 0x3a, 0xb1, 0xcb,
    0x3c, 0x03, 0x02, 0x9b, 0x5a, 0x05, 0xec, 0x88, 0x48, 0x54, 0x42,
    0xf1, 0x20, 0xb4, 0x75, 0x01, 0xde, 0x61, 0xf1, 0x39, 0x5d,
};
constexpr uint8_t kTestProtobuf[] = {
    0x0a, 0x10, 0x71, 0xfd, 0xf9, 0x65, 0xa8, 0x7c, 0x61, 0xe2, 0xff, 0x27,
    0x0c, 0x76, 0x25, 0x23, 0xe0, 0xa4, 0x12, 0x10, 0x77, 0xf2, 0x3c, 0x31,
    0x3c, 0xe8, 0x94, 0x9a, 0x9f, 0xbc, 0xdf, 0x44, 0xfc, 0xf5, 0x41, 0x97,
    0x1a, 0x0b, 0x77, 0x65, 0x62, 0x61, 0x75, 0x74, 0x68, 0x6e, 0x2e, 0x69,
    0x6f, 0x22, 0x06, 0x56, 0x47, 0x56, 0x7a, 0x64, 0x41, 0x2a, 0x10, 0x60,
    0x07, 0x19, 0x5b, 0x4e, 0x19, 0xf9, 0x6e, 0xc1, 0xfc, 0xfd, 0x0a, 0xf6,
    0x0c, 0x00, 0x7e, 0x30, 0xf9, 0xa0, 0xea, 0xf3, 0xc8, 0x31, 0x3a, 0x04,
    0x54, 0x65, 0x73, 0x74, 0x42, 0x04, 0x54, 0x65, 0x73, 0x74, 0x4a, 0xa6,
    0x01, 0xdc, 0xc5, 0x16, 0x15, 0x91, 0x24, 0xd2, 0x31, 0xfc, 0x85, 0x8b,
    0xe2, 0xec, 0x22, 0x09, 0x8f, 0x8d, 0x0f, 0xbe, 0x9b, 0x59, 0x71, 0x04,
    0xcd, 0xaa, 0x3d, 0x32, 0x23, 0xbd, 0x25, 0x46, 0x14, 0x86, 0x9c, 0xfe,
    0x74, 0xc8, 0xd3, 0x37, 0x70, 0xed, 0xb0, 0x25, 0xd4, 0x1b, 0xdd, 0xa4,
    0x3c, 0x02, 0x13, 0x8c, 0x69, 0x03, 0xff, 0xd1, 0xb0, 0x72, 0x00, 0x29,
    0xcf, 0x5f, 0x06, 0xb3, 0x94, 0xe2, 0xea, 0xca, 0x68, 0xdd, 0x0b, 0x07,
    0x98, 0x7a, 0x2c, 0x8f, 0x08, 0xee, 0x7d, 0xad, 0x16, 0x35, 0xc7, 0x10,
    0xf3, 0xa4, 0x90, 0x84, 0xd1, 0x8e, 0x2e, 0xdb, 0xb9, 0xfa, 0x72, 0x9a,
    0xcf, 0x12, 0x1b, 0x3c, 0xca, 0xfa, 0x79, 0x4a, 0x1e, 0x1b, 0xe1, 0x15,
    0xdf, 0xab, 0xee, 0x75, 0xbb, 0x5c, 0x5a, 0x94, 0x14, 0xeb, 0x72, 0xae,
    0x37, 0x97, 0x03, 0xa8, 0xe7, 0x62, 0x9d, 0x2e, 0xfd, 0x28, 0xce, 0x03,
    0x34, 0x20, 0xa7, 0xa2, 0x7b, 0x00, 0xc8, 0x12, 0x62, 0x12, 0x7f, 0x54,
    0x73, 0x8c, 0x21, 0xc8, 0x85, 0x15, 0xce, 0x36, 0x14, 0xd9, 0x41, 0x22,
    0xe8, 0xbf, 0x88, 0xf9, 0x45, 0xe4, 0x1c, 0x89, 0x7d, 0xa4, 0x23, 0x58,
    0x00, 0x68, 0x98, 0xf5, 0x81, 0xef, 0xad, 0xf4, 0xda, 0x17, 0x70, 0xab,
    0x03,
};
constexpr std::string_view kSampleRecoverableKeyStoreCertXML =
    R"(<?xml version="1.0" encoding="UTF-8"?>
<certificate>
  <metadata>
    <serial>10016</serial>
    <creation-time>1694037058</creation-time>
    <refresh-interval>2592000</refresh-interval>
    <previous>
      <serial>10015</serial>
      <hash>TQudrujnu1I9bdoDaYxGQYuRN/8SwTLjdk6vzYTOkIU=</hash>
    </previous>
  </metadata>
  <intermediates>
    <cert>MIIFCjCCAvKgAwIBAgIRAN7d1InOjWGTUT558zWPLwEwDQYJKoZIhvcNAQELBQAwIDEeMBwGA1UEAxMVR29vZ2xlIENyeXB0QXV0aFZhdWx0MB4XDTE4MDUwOTAxMjAwNloXDTI4MDUxMDAxMjAwNlowOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAO9067x94+sxIpqXHN9rcdorqVsH8s3ROZeBI3ORAWM8dGmR+m/yg7rrcLrLZNCKMo5RskjAc/9tWIFnoyJvp3bgJaZO1mOZGB6dF1rc3ZsWZJ5lk6roD3jWXoihI6A5qciG2Ojfn9d4UNkVYflg0xKMEP4tOFgS++XIbIZSBvtwONoOUK+w2RCnU/aCUKpJ7c49HBsieV/AcI3k4ia72JNip/9OeefyqaeuRt0X9vVTz1N4uu5LYQE90mrywaR9N0uFmfkJX6wIhkM4snbc/be5kpNcXn42seWVgLiQHwmynyN1VgHGlK+D+ewc5g3EotI4LNWjN7dgaz3wDEcVr9+cg2Z6wvh4qc5I8gxgXx5hYKIJcoXPXvyo95krrDtEatcILlVyrNoSl0aGhibh7Xt2CMEwtaS856r6JYQ9Zz6F3/KzM4B0c5XPR/Il7IAdae/e+Z4eVgj6zA19ngJmHWtMUzHHE3gcyDNqIcULMZYea7I11TVN4oW1pB6rsyIsBXALZXT93TJLI9HZ/w52A8qJIxIFP89iNtehPd8fYZipBJOj6e6PLf8+pcDE/RSSLs6ezURJ1gkovnubNhOxQ4+ku8WNsxCFB65sLriXNI8yZ8HWftJsop2k5gQ7wV0eXFNXJhAGaIXggKEb/Wf+qAEnMyxdAuLrlXwORl3AJteHAgMBAAGjJjAkMA4GA1UdDwEB/wQEAwIBhjASBgNVHRMBAf8ECDAGAQH/AgEBMA0GCSqGSIb3DQEBCwUAA4ICAQBlbWcXgD4KCBgBpNU6z8675oAiJb4YwrI8GT2Y5lglz6jkmy9gPZdU56PPyXO0MIBCsmmXxEcVURDULuX8DJsbzuqnbM8mEbmK8CVlMhq9NNOFZMCtnhu647lY+ZabBUYr4bSgPiJxwwMor3c15PFx/deZAYeAtbV9zW0Q07yXmjOoQhtgvJjEO9pwxwf1gktD9Wbj7OpSiLNlKGpLFOTjm0ckzIBGgwvYWp+A6LCjmOzuV91hdUF4LErG0Z6GQVllazHSJ5oaNEJx6wyJnt+gL4TDXwgDF7QpkSixBgfx5TY9QVsTi/wLzkDCjl8xuX3YXdlojofksxa83MAF6W8Pua4ZhKFTcnGAFQMTfPMUt0BAEkyTxlAovZ7H+ZXCkD47TkcGI9KWav7dDL9P4IqQljD9fr/R0anlH+rwJn9jJ1UqTbWoHgYr8qNa4SkD3WfZhb7TQJbUD6VocrEqBz6P9WgJFlB0Nn54ue7RlFC5+nlV8m6ZPbf6+f7wVOrVn0Obxq2t9RSiL9AebPDgfts+JgvflmPSOHD5W+4o42S4/huelfFxuIM1aid8lZip0TJBzYXWmOCp2SPHdN0wIp7/m1FjJ5Z7rjqn0dB+oXvHapywAdymEaVm/rs940d50cGg/1RfvAC3oYSyZe99YeK9DEQo1249+0n6QhhoJQJACw==</cert>
    <cert>MIIFGjCCAwKgAwIBAgIQHflnDNWkj2yxeD1IB6GdTTANBgkqhkiG9w0BAQsFADAxMS8wLQYDVQQDEyZHb29nbGUgQ2xvdWQgS2V5IFZhdWx0IFNlcnZpY2UgUm9vdCBDQTAeFw0xODA1MDcxODU4MTBaFw0yODA1MDgxODU4MTBaMDkxNzA1BgNVBAMTLkdvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBJbnRlcm1lZGlhdGUgQ0EwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQDvdOu8fePrMSKalxzfa3HaK6lbB/LN0TmXgSNzkQFjPHRpkfpv8oO663C6y2TQijKOUbJIwHP/bViBZ6Mib6d24CWmTtZjmRgenRda3N2bFmSeZZOq6A941l6IoSOgOanIhtjo35/XeFDZFWH5YNMSjBD+LThYEvvlyGyGUgb7cDjaDlCvsNkQp1P2glCqSe3OPRwbInlfwHCN5OImu9iTYqf/Tnnn8qmnrkbdF/b1U89TeLruS2EBPdJq8sGkfTdLhZn5CV+sCIZDOLJ23P23uZKTXF5+NrHllYC4kB8Jsp8jdVYBxpSvg/nsHOYNxKLSOCzVoze3YGs98AxHFa/fnINmesL4eKnOSPIMYF8eYWCiCXKFz178qPeZK6w7RGrXCC5VcqzaEpdGhoYm4e17dgjBMLWkvOeq+iWEPWc+hd/yszOAdHOVz0fyJeyAHWnv3vmeHlYI+swNfZ4CZh1rTFMxxxN4HMgzaiHFCzGWHmuyNdU1TeKFtaQeq7MiLAVwC2V0/d0ySyPR2f8OdgPKiSMSBT/PYjbXoT3fH2GYqQSTo+nujy3/PqXAxP0Uki7Ons1ESdYJKL57mzYTsUOPpLvFjbMQhQeubC64lzSPMmfB1n7SbKKdpOYEO8FdHlxTVyYQBmiF4IChG/1n/qgBJzMsXQLi65V8DkZdwCbXhwIDAQABoyYwJDAOBgNVHQ8BAf8EBAMCAYYwEgYDVR0TAQH/BAgwBgEB/wIBATANBgkqhkiG9w0BAQsFAAOCAgEAQ+G3v3JCbzChBs8HUGx6i2TMm1NZM71+chbA2JF9De8kVd/r2CETvvBRLXcTPcWWA0+PRDGaDmi4TR3bJhXgBStecQZkQtzI3ZcdFfI0rTNeCevfHp5nJjtB+AYomCTKNrlNLpk9YbJosqEKVLQBhlLNYm3PT4CQYJ1NubLLtKF1cn4Z+eayxud1kDrZWFyN5CYewOrtXc8oCynj8H0/NydOuCRQU2c/UXWmvsmlRRffHJEXLqCMitTHV9w4VHEVg9YYssxno/jWtp+b4z8JsE2vkJjs2tmOvfiMupbJx9h6zj2j04rjhf/A+vGPRKOD5WtbbX4An2+szsNLmERBfWUNsO1AaSTc3W+AJOjrG30tewS7jFRPluTtgB+kmozSW0MU/BgAYJuNKRVP8zklVmQqJRbrrxSzrvHzJlz/lvFu9MD7nGtiFqT9VggFjqq5vgn5srBp3Dq4GDGerg+HCDCN9qgnL1gBcKzCMK1oT0bCRWZGckT28WMnfcgZ/fuEVNgQcEXLgWiZWZDBEVlMh7u2QoOr2LXwXuXME8k87rAQbxvGLhyxq2uNxUdH16uljm7p5u2Qobyqxqf2rOGJYCBLK2JP74d6Nl6hD5FGBBaO6mN0Ojn/ShJ1Cq9o3wCHoLYn55wJnXYu7QXAX6230h7ekXpbxPPHO4x0Var5p+8=</cert>
  </intermediates>
  <endpoints>
    <cert>MIIDOzCCASOgAwIBAgIRALohAkmP2SJK75Xsk8FsngUwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUwNThaFw0yNTA0MDkwMDAwMDBaMDIxMDAuBgNVBAMTJ0dvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBFbmRwb2ludDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABMCD3sSR26q9occ1Y/K2SQyIsSJkJtGALvd3t4l9E8ajmOV9fQHp7d4ExmRJIldlFL/Y5i5FBg3NvwK7TLvoAPmjEDAOMAwGA1UdEwEB/wQCMAAwDQYJKoZIhvcNAQELBQADggIBAD7HLz0sS04rV7BXzrd2KJdMk2fCbrjTPNNUUZu+UbPB0lDvWcP1+uroIOEZuPLUK0EBbQYzCjP/bp7tT4me4myivPbg2IBLvTaOVKbUzi6SqA4X+vyAe3c7Bp6A3hPzxNangk2jmpKdIvLXJ8DHyXVrCXk/dNObnWUDnvbmoXg5yWK/snB5OIysDPUlxUmRspxhRajVgRnDAMTnJ2YZhHC15Jm/neugxVKeSeBb4wamLRibkdWbc4KJTiSjh1CnH1OKsCI8N006Gk+YXHnrY3OmakVg/bSnfAoMWLMDvtXbDbMVYAl9uRLBDwoOS6MFMsrj+Iwniuv4E2Kb+UcWK36AR/KH1/ILFpRUTtfPwIQcvEc2tWkH+W2BJqKOvwGH3rOm2qF88g8/egrHua7jnv8aJlfQ3c3S7ytikxugCQhSAJhVO0kdWXGUut78UzBrhMEvBqHlQtZnyPSEWd6bJKdGqwmbQwdKoou5HCu0YQxanmzENR9PmDs6+AMN0xJDcb9TOBQsvQW+vY3D34U61izaU2xytglgRzjSlBwFYDP75VgsL9gcNlYSt9R1EroPPsaEV1xhW47WpWArLdprVhVX70kPf3fUkcpDXimapFpMWONWlSUCIKPy/q0d2DcamL9HN5sZLyOGPctMTEowPomW8TiISWJFdtSK2fJXkk8s</cert>
    <cert>MIIDOzCCASOgAwIBAgIRALohAkmP2SJK75Xsk8FsngUwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUwNThaFw0yNTA0MDkwMDAwMDBaMDIxMDAuBgNVBAMTJ0dvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBFbmRwb2ludDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABOHSWq/RFpU1VnCCCmPcTDeJT3t3+27+BjFOdsC8/hcnbFUKwHt6Tt0uiHV3LP/aO0/DHYC8Kdb/KAMC+ai+aJ2jEDAOMAwGA1UdEwEB/wQCMAAwDQYJKoZIhvcNAQELBQADggIBALz6PK44f46capH7isFvHMdTosG3DIV4QP70zLtGtGBM+57RKU0UYLtgdtKfCCwQVIgru9PfMdNdbxKojI96cfB/QxsH5H/96iUET+EnvvQ63NMSnLtOp7H4UceBujpXeSLN0yRNr59JS+mLtyL5+5KjHgtOM7tpxJ3eP1tx8NnE30TE0BoeTQyoKu0wfHVsc5+Fs3EWJUpgV+Z0/KJFoy3M2Z0DHZxfn6fg+/xYxn8ttkMhlZXhJMjNqtcGmlwLYktmsG5LlsQNimXwGl9olVviEZwcHGUzHw8QWszoKzn+TgTgv76m2eZ5MwJeN1JnaLb+1gQtgKRpnG8TFxWGC/TIHUqLow/GruH2TSlLPr6l6ed+QjG01sAN5cdI7OR84D8W1F0vb8fVOr7kjf7N3qLDNQXDCRUUKHlRVanIt6h+kT1ctlM51+QmRhDsAkzY/3lFrXDySnQk18vlzTyA+QgqmvfNkPhgCp/fpgtWJFaPL9bJWaMaW/soXRUf26F6RMLK43EihdoVMtUAvmCIKUQyI88X6hJxEhWLyy/8Y45nAFk5CgXuzV2doOJTSITtJligTy1IuczH75bmp87c5ZPp51vUO4WYXuwffTCoQ8UYSYbNxxqKOfFkILnM1WoGAzCrVt5aKOyGPILzOsOS8X0EeQ9YF6Mvaf2iFljc2o30</cert>
    <cert>MIIDOzCCASOgAwIBAgIRALohAkmP2SJK75Xsk8FsngUwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUwNThaFw0yNTA0MDkwMDAwMDBaMDIxMDAuBgNVBAMTJ0dvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBFbmRwb2ludDBZMBMGByqGSM49AgEGCCqGSM49AwEHA0IABNeVqPpEctoVzN48WNefTpJEmRrrbpXoWRhHwH/AOYmQgXR6xX/AE1/qeen8fMj4Lnyb8KPveZjXvTlFq2mdBHGjEDAOMAwGA1UdEwEB/wQCMAAwDQYJKoZIhvcNAQELBQADggIBAEQIGwhKa7MDq+Wt5p7fvv1AXhX4HxpgkKv5xbuMWCcw6R8zTYQ4hF/XHegIEqjmwWFxEvD95Lu3oLz4gMEoZVywBt2QFb1wkWUjdeT9oy5YbrJiLm9evhMFWyjnu2h9OVqxCVvarVx35ZySThDr2n3CYntLSKyTSdVlzCsdcCOj1UFkqMe73gOUZFMkXETUoINlFYwX6NP5V1Moy8OjsSNa6/8zyYwivm3rQlj3GUEhSlX+0ib+IXYpcrDFF7/6+G8lWBAHmKGwGR6kpAQ7Zg7KEjY0gSYWOr86oJIMFzeXVjaqhwGXK2tO+JBTPZSf4zljke+QCDN1uZjscgpOOXcBvT3LqLDaz2TSen4EMXhD56lYrq/970a1ol7B26nNAjJr1Q2ZyH4kXgBnK/b7AjYzNhTx0k0o7zRdh4tMeNkxhHgpBQ7d8VM81lZJg95n5SuOvJkJlEsPus9nJ1QeKAAjLV+Hp4n+xEImnvwnPEeE9vo07KHeHsCaBFVVan+9VKMiFEnYO+JdA8DwVTwTHHRH2T2OcEF+oo6m9nZZgGZbcovftryoOetJRY8E2JG+j5ScVWwnh5QcWhP1oOqsZdFWbKmJyxbN0qhKRWB1l6xZipMTj4RYzrZtwXNWdJIudC1Lkr6GgMn2UybLPc4xDH5FLWDtLN7griLweFrniuAQ</cert>
  </endpoints>
</certificate>
)";
constexpr std::string_view kSampleRecoverableKeyStoreSigXML = R"(
<?xml version="1.0" encoding="UTF-8"?>
<signature>
  <intermediates>
    <cert>MIIFGjCCAwKgAwIBAgIQHflnDNWkj2yxeD1IB6GdTTANBgkqhkiG9w0BAQsFADAxMS8wLQYDVQQDEyZHb29nbGUgQ2xvdWQgS2V5IFZhdWx0IFNlcnZpY2UgUm9vdCBDQTAeFw0xODA1MDcxODU4MTBaFw0yODA1MDgxODU4MTBaMDkxNzA1BgNVBAMTLkdvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBJbnRlcm1lZGlhdGUgQ0EwggIiMA0GCSqGSIb3DQEBAQUAA4ICDwAwggIKAoICAQDvdOu8fePrMSKalxzfa3HaK6lbB/LN0TmXgSNzkQFjPHRpkfpv8oO663C6y2TQijKOUbJIwHP/bViBZ6Mib6d24CWmTtZjmRgenRda3N2bFmSeZZOq6A941l6IoSOgOanIhtjo35/XeFDZFWH5YNMSjBD+LThYEvvlyGyGUgb7cDjaDlCvsNkQp1P2glCqSe3OPRwbInlfwHCN5OImu9iTYqf/Tnnn8qmnrkbdF/b1U89TeLruS2EBPdJq8sGkfTdLhZn5CV+sCIZDOLJ23P23uZKTXF5+NrHllYC4kB8Jsp8jdVYBxpSvg/nsHOYNxKLSOCzVoze3YGs98AxHFa/fnINmesL4eKnOSPIMYF8eYWCiCXKFz178qPeZK6w7RGrXCC5VcqzaEpdGhoYm4e17dgjBMLWkvOeq+iWEPWc+hd/yszOAdHOVz0fyJeyAHWnv3vmeHlYI+swNfZ4CZh1rTFMxxxN4HMgzaiHFCzGWHmuyNdU1TeKFtaQeq7MiLAVwC2V0/d0ySyPR2f8OdgPKiSMSBT/PYjbXoT3fH2GYqQSTo+nujy3/PqXAxP0Uki7Ons1ESdYJKL57mzYTsUOPpLvFjbMQhQeubC64lzSPMmfB1n7SbKKdpOYEO8FdHlxTVyYQBmiF4IChG/1n/qgBJzMsXQLi65V8DkZdwCbXhwIDAQABoyYwJDAOBgNVHQ8BAf8EBAMCAYYwEgYDVR0TAQH/BAgwBgEB/wIBATANBgkqhkiG9w0BAQsFAAOCAgEAQ+G3v3JCbzChBs8HUGx6i2TMm1NZM71+chbA2JF9De8kVd/r2CETvvBRLXcTPcWWA0+PRDGaDmi4TR3bJhXgBStecQZkQtzI3ZcdFfI0rTNeCevfHp5nJjtB+AYomCTKNrlNLpk9YbJosqEKVLQBhlLNYm3PT4CQYJ1NubLLtKF1cn4Z+eayxud1kDrZWFyN5CYewOrtXc8oCynj8H0/NydOuCRQU2c/UXWmvsmlRRffHJEXLqCMitTHV9w4VHEVg9YYssxno/jWtp+b4z8JsE2vkJjs2tmOvfiMupbJx9h6zj2j04rjhf/A+vGPRKOD5WtbbX4An2+szsNLmERBfWUNsO1AaSTc3W+AJOjrG30tewS7jFRPluTtgB+kmozSW0MU/BgAYJuNKRVP8zklVmQqJRbrrxSzrvHzJlz/lvFu9MD7nGtiFqT9VggFjqq5vgn5srBp3Dq4GDGerg+HCDCN9qgnL1gBcKzCMK1oT0bCRWZGckT28WMnfcgZ/fuEVNgQcEXLgWiZWZDBEVlMh7u2QoOr2LXwXuXME8k87rAQbxvGLhyxq2uNxUdH16uljm7p5u2Qobyqxqf2rOGJYCBLK2JP74d6Nl6hD5FGBBaO6mN0Ojn/ShJ1Cq9o3wCHoLYn55wJnXYu7QXAX6230h7ekXpbxPPHO4x0Var5p+8=</cert>
    <cert>MIIFCjCCAvKgAwIBAgIRAN7d1InOjWGTUT558zWPLwEwDQYJKoZIhvcNAQELBQAwIDEeMBwGA1UEAxMVR29vZ2xlIENyeXB0QXV0aFZhdWx0MB4XDTE4MDUwOTAxMjAwNloXDTI4MDUxMDAxMjAwNlowOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAO9067x94+sxIpqXHN9rcdorqVsH8s3ROZeBI3ORAWM8dGmR+m/yg7rrcLrLZNCKMo5RskjAc/9tWIFnoyJvp3bgJaZO1mOZGB6dF1rc3ZsWZJ5lk6roD3jWXoihI6A5qciG2Ojfn9d4UNkVYflg0xKMEP4tOFgS++XIbIZSBvtwONoOUK+w2RCnU/aCUKpJ7c49HBsieV/AcI3k4ia72JNip/9OeefyqaeuRt0X9vVTz1N4uu5LYQE90mrywaR9N0uFmfkJX6wIhkM4snbc/be5kpNcXn42seWVgLiQHwmynyN1VgHGlK+D+ewc5g3EotI4LNWjN7dgaz3wDEcVr9+cg2Z6wvh4qc5I8gxgXx5hYKIJcoXPXvyo95krrDtEatcILlVyrNoSl0aGhibh7Xt2CMEwtaS856r6JYQ9Zz6F3/KzM4B0c5XPR/Il7IAdae/e+Z4eVgj6zA19ngJmHWtMUzHHE3gcyDNqIcULMZYea7I11TVN4oW1pB6rsyIsBXALZXT93TJLI9HZ/w52A8qJIxIFP89iNtehPd8fYZipBJOj6e6PLf8+pcDE/RSSLs6ezURJ1gkovnubNhOxQ4+ku8WNsxCFB65sLriXNI8yZ8HWftJsop2k5gQ7wV0eXFNXJhAGaIXggKEb/Wf+qAEnMyxdAuLrlXwORl3AJteHAgMBAAGjJjAkMA4GA1UdDwEB/wQEAwIBhjASBgNVHRMBAf8ECDAGAQH/AgEBMA0GCSqGSIb3DQEBCwUAA4ICAQBlbWcXgD4KCBgBpNU6z8675oAiJb4YwrI8GT2Y5lglz6jkmy9gPZdU56PPyXO0MIBCsmmXxEcVURDULuX8DJsbzuqnbM8mEbmK8CVlMhq9NNOFZMCtnhu647lY+ZabBUYr4bSgPiJxwwMor3c15PFx/deZAYeAtbV9zW0Q07yXmjOoQhtgvJjEO9pwxwf1gktD9Wbj7OpSiLNlKGpLFOTjm0ckzIBGgwvYWp+A6LCjmOzuV91hdUF4LErG0Z6GQVllazHSJ5oaNEJx6wyJnt+gL4TDXwgDF7QpkSixBgfx5TY9QVsTi/wLzkDCjl8xuX3YXdlojofksxa83MAF6W8Pua4ZhKFTcnGAFQMTfPMUt0BAEkyTxlAovZ7H+ZXCkD47TkcGI9KWav7dDL9P4IqQljD9fr/R0anlH+rwJn9jJ1UqTbWoHgYr8qNa4SkD3WfZhb7TQJbUD6VocrEqBz6P9WgJFlB0Nn54ue7RlFC5+nlV8m6ZPbf6+f7wVOrVn0Obxq2t9RSiL9AebPDgfts+JgvflmPSOHD5W+4o42S4/huelfFxuIM1aid8lZip0TJBzYXWmOCp2SPHdN0wIp7/m1FjJ5Z7rjqn0dB+oXvHapywAdymEaVm/rs940d50cGg/1RfvAC3oYSyZe99YeK9DEQo1249+0n6QhhoJQJACw==</cert>
  </intermediates>
  <certificate>MIIFGTCCAwGgAwIBAgIRAOUOMMnP/H98t0zAwO3YjxIwDQYJKoZIhvcNAQELBQAwOTE3MDUGA1UEAxMuR29vZ2xlIENsb3VkIEtleSBWYXVsdCBTZXJ2aWNlIEludGVybWVkaWF0ZSBDQTAeFw0yMzA5MDUyMTUxMDBaFw0yODA5MDYyMTUxMDBaMDUxMzAxBgNVBAMTKkdvb2dsZSBDbG91ZCBLZXkgVmF1bHQgU2VydmljZSBTaWduaW5nIEtleTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBANqoaDjGHUrdnO6raw9omQ+xnhSxqwTSY2dlC83an+F9JNlL/CHjvn+kyKP7rP57k4y9+9REqjvk+zaR6rQjzP6m2FbYf/kXsmS8ohtTXsmI9NTvobGCGZOYwFbB28yxoOiXA2A91cG+Rt/KmetMcGphFE0/9PGZg9JSmWiGLDJEvgG4ckz6fmL/orhbC/V1K3ArNZ2eJ8Sw29eMo62XpJqvmi+6BrFS3edcJNC1dUpC/ixP73G1J5XDVb60no4JolG1N7Utug/WlPr88eI7LdV05sMfRfX+ta4TrIK7yJ1urGuOVsIDBGFjsfgpRTlwiG829D9uGhRSAE8GzVCFiVF8AfQwlEtgahwg23QzWRaKYo6qeRMCw1hNURF31hQ5bgQeKcaS98x6MkzszBOT2aFiK0EWBzwsJLI3KadRYUMcKa3AFXSv7QLGkAU+Ivas/m3Mt0s7KQnIzjsYbOqiC895WsylxaQyMy5xvVKp0gYjmK2YtgfXo59hznqns1FzeR4fBsbKsh+NnWXzcJ8cEg8jbk0nxAz0reMj1IN25Wb1WDfUCiTy+9V6dfFLQFQ6KYDb/bbIRyPk4g176gWK9agVrHrhiQsDVstSN/cAgLBVUFi1oeLzZ0SwB4wCXuP8SmEVrGl3zxxv3szgUxwfm+elaZ0BrA5deSenJdhV1QQ3AgMBAAGjIDAeMA4GA1UdDwEB/wQEAwIHgDAMBgNVHRMBAf8EAjAAMA0GCSqGSIb3DQEBCwUAA4ICAQDuLSK5nov/grmYNc8CTnrKNZ1w8p5Wbi9QThzJXoSV1BuFklXNX4GlgjZ04eS5ns/lUCdqByx0K2ZGX24wzZX0sSUQ+74Fq5uDINm6ESPV46y6hXvqIotLYIrgpl7Z2Ej7D6JT5fPYzAncUQd8Z9LuNMMt/rG8IlfSN6yOuZnAxI8wKtCrp23QugtqYKHyfxCN/HzCMEs1XP7qhgolnmLoTqU9j2HlPPESmH4+St4w7QPVQWARQ2S0hdtT4dhjmkqeDBojBjkGn9fS+vsOKsH3CDTt3A0pFI66xQ9TwT5mHCIIkAxGzc/DzPtpTUz6XBhtWNyI59adbCHfOtWWNjpriYvTbOm1ZZL6DXsaFJIbYX0Cmh6unonuvZ2c1Pu6nnVxR1HamIdtDZjvgbyFRJ4wCWpMhAU9WVJSotz57OXf/CvbBI0gfhl/EmWtKsGiDryPjphILWrnO55V6G6HJgk6xpzcjZzSnWpf5UF9RGjUaZNwOtxma/57pM8o5vTCeaOrq/3dKUWO2JBgxkOG+/ZCOe0E0Q2CwCCWTtf4ReaUIbeYQTj4cfR4eaj6Z8euytwEM2UQCep+HXJdOxv6/eHRXPK21Alt0crWmhZ8J7hZyeZ/24a3in8hqg9X9wxZXPghXo4W3My3Tn+dP2m36RiBQOCHSoYWMRINZccj9284GQ==</certificate>
  <value>n6kI2dGZKz5CGbXnbz79m51QTDt+WszzNOvcqXsGm6g3ObmpjkghTU3wPmrJ0c5zUD1l4QQEmTKRBIACgK7Sp64JdC4IGP5y+z8HhXPslP3Dc5aySOk4b++m7AIbkAuw63SbPD8L2nQ20CMNiaVVBqZJ0uWUV04qN8IOll1L8NbeZLhjFUcx9riYBrzWOr9uis5IANkfPTFgFyPFjqFk9XrbVpPcNCRtz7Pew+L7OW5z7sh5rW8iZmjhhV/e4VDTgYBFq/Js5W4yalRI9uuEXLJqG1/US4L5cMnJoZOxPmz48an0ug/Pi8yV9cIq+xvER/XaeeUG53Fqy9cn2qG6ROwxH109toaLx3TZaLjdVh7wcJCLtOY6WngHksQbIyU1mDYzz7uWItCss2Nb0NbZ+QMn3k1GxDGIwlY/HXdt7OihPQWLRM2H/QRqlI9p8i1L+DaPrhyGrGHzYKN8z9qGZYx1AsQUWQCR0YeXvlxjtSvBEPtWkfEE0RrZPJtFh+bvrD55Id7XapnGKKXYMmYf9KbDJ3GMD1aT6xgMhlAhtltN5vNg08LSH5Ma4TXhmNpKny5JQqlAUTby1wIhgdElQSdU0jYpmle8N0wsuLoX+e3bHFKxWVkrwvXDC0v2wqH5mzm8FLhxXZDA2ApnGT+eOC1gjd8qTuouzm5GuMhjvig=</value>
</signature>
)";

std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> GetTestEntity() {
  auto ret = std::make_unique<sync_pb::WebauthnCredentialSpecifics>();
  CHECK(ret->ParseFromArray(kTestProtobuf, sizeof(kTestProtobuf)));
  return ret;
}

std::string StringOfZeros(size_t len) {
  return std::string(len, '0');
}

webauthn_pb::EnclaveLocalState::WrappedPIN GetTestWrappedPIN() {
  webauthn_pb::EnclaveLocalState::WrappedPIN wrapped_pin;
  wrapped_pin.set_wrapped_pin(StringOfZeros(30));
  wrapped_pin.set_claim_key(StringOfZeros(32));
  wrapped_pin.set_generation(0);
  wrapped_pin.set_form(wrapped_pin.FORM_SIX_DIGITS);
  wrapped_pin.set_hash(wrapped_pin.HASH_SCRYPT);
  wrapped_pin.set_hash_difficulty(1 << 12);
  wrapped_pin.set_hash_salt(StringOfZeros(16));

  return wrapped_pin;
}

struct TempDir {
 public:
  TempDir() { CHECK(dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() const { return dir_.GetPath(); }

 private:
  base::ScopedTempDir dir_;
};

std::pair<base::Process, uint16_t> StartEnclave(base::FilePath cwd) {
  base::FilePath data_root;
  CHECK(base::PathService::Get(base::DIR_OUT_TEST_DATA_ROOT, &data_root));
  const base::FilePath enclave_bin_path =
      data_root.AppendASCII("cloud_authenticator_test_service");
  base::LaunchOptions subprocess_opts;
  subprocess_opts.current_directory = cwd;

  std::optional<base::Process> enclave_process;
  uint16_t port;
  char port_str[6];

  for (int i = 0; i < 10; i++) {
#if BUILDFLAG(IS_WIN)
    HANDLE read_handle;
    HANDLE write_handle;
    SECURITY_ATTRIBUTES security_attributes;

    security_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    security_attributes.bInheritHandle = TRUE;
    security_attributes.lpSecurityDescriptor = NULL;
    CHECK(CreatePipe(&read_handle, &write_handle, &security_attributes, 0));

    subprocess_opts.stdin_handle = INVALID_HANDLE_VALUE;
    subprocess_opts.stdout_handle = write_handle;
    subprocess_opts.stderr_handle = INVALID_HANDLE_VALUE;
    subprocess_opts.handles_to_inherit.push_back(write_handle);
    enclave_process = base::LaunchProcess(base::CommandLine(enclave_bin_path),
                                          subprocess_opts);
    CloseHandle(write_handle);
    CHECK(enclave_process->IsValid());

    DWORD read_bytes;
    CHECK(ReadFile(read_handle, port_str, sizeof(port_str), &read_bytes, NULL));
    CloseHandle(read_handle);
#else
    int fds[2];
    CHECK(!pipe(fds));
    subprocess_opts.fds_to_remap.emplace_back(fds[1], 1);
    enclave_process = base::LaunchProcess(base::CommandLine(enclave_bin_path),
                                          subprocess_opts);
    CHECK(enclave_process->IsValid());
    close(fds[1]);

    const ssize_t read_bytes =
        HANDLE_EINTR(read(fds[0], port_str, sizeof(port_str)));
    close(fds[0]);
#endif

    CHECK(read_bytes > 0);
    port_str[read_bytes - 1] = 0;
    unsigned u_port;
    CHECK(base::StringToUint(port_str, &u_port)) << port_str;
    port = base::checked_cast<uint16_t>(u_port);

    if (net::IsPortAllowedForScheme(port, "wss")) {
      break;
    }
    LOG(INFO) << "Port " << port << " not allowed. Trying again.";

    // The kernel randomly picked a port that Chromium will refuse to connect
    // to. Try again.
    enclave_process->Terminate(/*exit_code=*/1, /*wait=*/false);
  }

  return std::make_pair(std::move(*enclave_process), port);
}

enclave::ScopedEnclaveOverride TestEnclaveIdentity(uint16_t port) {
  constexpr std::array<uint8_t, device::kP256X962Length> kTestPublicKey = {
      0x04, 0x6b, 0x17, 0xd1, 0xf2, 0xe1, 0x2c, 0x42, 0x47, 0xf8, 0xbc,
      0xe6, 0xe5, 0x63, 0xa4, 0x40, 0xf2, 0x77, 0x03, 0x7d, 0x81, 0x2d,
      0xeb, 0x33, 0xa0, 0xf4, 0xa1, 0x39, 0x45, 0xd8, 0x98, 0xc2, 0x96,
      0x4f, 0xe3, 0x42, 0xe2, 0xfe, 0x1a, 0x7f, 0x9b, 0x8e, 0xe7, 0xeb,
      0x4a, 0x7c, 0x0f, 0x9e, 0x16, 0x2b, 0xce, 0x33, 0x57, 0x6b, 0x31,
      0x5e, 0xce, 0xcb, 0xb6, 0x40, 0x68, 0x37, 0xbf, 0x51, 0xf5,
  };
  const std::string url = "ws://127.0.0.1:" + base::NumberToString(port);
  enclave::EnclaveIdentity identity;
  identity.url = GURL(url);
  identity.public_key = kTestPublicKey;

  return enclave::ScopedEnclaveOverride(std::move(identity));
}

std::string MakeVaultResponse() {
  trusted_vault_pb::Vault vault;
  vault.mutable_vault_parameters()->set_vault_handle("test vault handle");
  return vault.SerializeAsString();
}

std::unique_ptr<network::NetworkService> CreateNetwork(
    mojo::Remote<network::mojom::NetworkContext>* network_context) {
  network::mojom::NetworkContextParamsPtr params =
      network::mojom::NetworkContextParams::New();
  params->cert_verifier_params =
      network::FakeTestCertVerifierParamsFactory::GetCertVerifierParams();

  auto service = network::NetworkService::CreateForTesting();
  service->CreateNetworkContext(network_context->BindNewPipeAndPassReceiver(),
                                std::move(params));

  return service;
}

scoped_refptr<device::JSONRequest> JSONFromString(base::StringPiece json_str) {
  base::Value json_request = base::JSONReader::Read(json_str).value();
  return base::MakeRefCounted<device::JSONRequest>(std::move(json_request));
}

class EnclaveManagerTest : public testing::Test, EnclaveManager::Observer {
 public:
  EnclaveManagerTest()
      // `IdentityTestEnvironment` wants to run on an IO thread.
      : task_env_(base::test::TaskEnvironment::MainThreadType::IO),
        temp_dir_(),
        process_and_port_(StartEnclave(temp_dir_.GetPath())),
        enclave_override_(TestEnclaveIdentity(process_and_port_.second)),
        network_service_(CreateNetwork(&network_context_)),
        security_domain_service_(
            FakeSecurityDomainService::New(kSecretVersion)),
        manager_(temp_dir_.GetPath(),
                 identity_test_env_.identity_manager(),
                 network_context_.get(),
                 url_loader_factory_.GetSafeWeakWrapper()) {
    OSCryptMocker::SetUp();

    identity_test_env_.MakePrimaryAccountAvailable(
        "test@gmail.com", signin::ConsentLevel::kSignin);
    gaia_id_ = identity_test_env_.identity_manager()
                   ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
                   .gaia;
    identity_test_env_.SetAutomaticIssueOfAccessTokens(true);
    manager_.AddObserver(this);

    auto security_domain_service_callback =
        security_domain_service_->GetCallback();
    url_loader_factory_.SetInterceptor(base::BindLambdaForTesting(
        [callback = std::move(security_domain_service_callback),
         this](const network::ResourceRequest& request) {
          std::optional<std::pair<net::HttpStatusCode, std::string>> response =
              callback.Run(request);
          if (response) {
            url_loader_factory_.AddResponse(request.url.spec(),
                                            std::move(response->second),
                                            response->first);
          }
        }));
    mock_hw_provider_ =
        std::make_unique<crypto::ScopedMockUnexportableKeyProvider>();
  }

  ~EnclaveManagerTest() override {
    if (manager_.RunWhenStoppedForTesting(task_env_.QuitClosure())) {
      task_env_.RunUntilQuit();
    }
    CHECK(process_and_port_.first.Terminate(/*exit_code=*/1, /*wait=*/true));
    OSCryptMocker::TearDown();
  }

 protected:
  base::flat_set<std::string> GaiaAccountsInState() const {
    const webauthn_pb::EnclaveLocalState& state =
        manager_.local_state_for_testing();
    base::flat_set<std::string> ret;
    for (const auto& it : state.users()) {
      ret.insert(it.first);
    }
    return ret;
  }

  void OnKeysStored() override { stored_count_++; }

  void DoCreate(
      std::unique_ptr<enclave::ClaimedPIN> claimed_pin,
      std::unique_ptr<sync_pb::WebauthnCredentialSpecifics>* out_specifics) {
    auto ui_request = std::make_unique<enclave::CredentialRequest>();
    ui_request->signing_callback = manager_.HardwareKeySigningCallback();
    int32_t secret_version;
    std::vector<uint8_t> wrapped_secret;
    std::tie(secret_version, wrapped_secret) =
        manager_.GetCurrentWrappedSecret();
    EXPECT_EQ(secret_version, kSecretVersion);
    ui_request->wrapped_secrets = {std::move(wrapped_secret)};
    ui_request->wrapped_secret_version = kSecretVersion;
    ui_request->claimed_pin = std::move(claimed_pin);

    std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> specifics;

    enclave::EnclaveAuthenticator authenticator(
        std::move(ui_request), /*save_passkey_callback=*/
        base::BindLambdaForTesting(
            [&specifics](sync_pb::WebauthnCredentialSpecifics in_specifics) {
              specifics =
                  std::make_unique<sync_pb::WebauthnCredentialSpecifics>(
                      std::move(in_specifics));
            }),
        network_context_.get());

    std::vector<device::PublicKeyCredentialParams::CredentialInfo>
        pub_key_params;
    pub_key_params.emplace_back(
        device::PublicKeyCredentialParams::CredentialInfo());

    device::MakeCredentialOptions ctap_options;
    ctap_options.json = JSONFromString(R"({
        "attestation": "none",
        "authenticatorSelection": {
          "residentKey": "preferred",
          "userVerification": "preferred"
        },
        "challenge": "xHyLYEorFsaL6vb",
        "extensions": { "credProps": true },
        "pubKeyCredParams": [
          { "alg": -7, "type": "public-key" },
          { "alg": -257, "type": "public-key" }
        ],
        "rp": {
          "id": "webauthn.io",
          "name": "webauthn.io"
        },
        "user": {
          "displayName": "test",
          "id": "ZEdWemRB",
          "name": "test"
        }
      })");

    auto quit_closure = task_env_.QuitClosure();
    std::optional<device::CtapDeviceResponseCode> status;
    std::optional<device::AuthenticatorMakeCredentialResponse> response;
    authenticator.MakeCredential(
        /*request=*/{R"({"foo": "bar"})",
                     /*rp=*/{"rpid", "rpname"},
                     /*user=*/{{'u', 'i', 'd'}, "user", "display name"},
                     device::PublicKeyCredentialParams(
                         std::move(pub_key_params))},
        std::move(ctap_options),
        base::BindLambdaForTesting(
            [&quit_closure, &status, &response](
                device::CtapDeviceResponseCode in_status,
                std::optional<device::AuthenticatorMakeCredentialResponse>
                    in_responses) {
              status = in_status;
              response = std::move(in_responses);
              quit_closure.Run();
            }));
    task_env_.RunUntilQuit();

    ASSERT_TRUE(status.has_value());
    ASSERT_EQ(status, device::CtapDeviceResponseCode::kSuccess);
    ASSERT_TRUE(response.has_value());
    ASSERT_TRUE(specifics);
    EXPECT_EQ(specifics->rp_id(), "rpid");
    EXPECT_EQ(specifics->user_id(), "uid");
    EXPECT_EQ(specifics->user_name(), "user");
    EXPECT_EQ(specifics->user_display_name(), "display name");
    EXPECT_EQ(specifics->key_version(), kSecretVersion);

    if (out_specifics) {
      *out_specifics = std::move(specifics);
    }
  }

  void DoAssertion(std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> entity,
                   std::unique_ptr<enclave::ClaimedPIN> claimed_pin) {
    auto ui_request = std::make_unique<enclave::CredentialRequest>();
    ui_request->signing_callback = manager_.HardwareKeySigningCallback();
    ui_request->wrapped_secrets = {
        *manager_.GetWrappedSecret(/*version=*/kSecretVersion)};
    ui_request->entity = std::move(entity);
    ui_request->claimed_pin = std::move(claimed_pin);

    enclave::EnclaveAuthenticator authenticator(
        std::move(ui_request), /*save_passkey_callback=*/
        base::BindRepeating(
            [](sync_pb::WebauthnCredentialSpecifics) { NOTREACHED(); }),
        network_context_.get());

    device::CtapGetAssertionRequest ctap_request("test.com",
                                                 R"({"foo": "bar"})");
    ctap_request.allow_list.emplace_back(device::PublicKeyCredentialDescriptor(
        device::CredentialType::kPublicKey, /*id=*/{1, 2, 3, 4}));

    device::CtapGetAssertionOptions ctap_options;
    ctap_options.json = JSONFromString(R"({
        "allowCredentials": [ ],
        "challenge": "CYO8B30gOPIOVFAaU61J7PvoETG_sCZQ38Gzpu",
        "rpId": "webauthn.io",
        "userVerification": "preferred"
    })");

    auto quit_closure = task_env_.QuitClosure();
    std::optional<device::CtapDeviceResponseCode> status;
    std::vector<device::AuthenticatorGetAssertionResponse> responses;
    authenticator.GetAssertion(
        std::move(ctap_request), std::move(ctap_options),
        base::BindLambdaForTesting(
            [&quit_closure, &status, &responses](
                device::CtapDeviceResponseCode in_status,
                std::vector<device::AuthenticatorGetAssertionResponse>
                    in_responses) {
              status = in_status;
              responses = std::move(in_responses);
              quit_closure.Run();
            }));
    task_env_.RunUntilQuit();

    ASSERT_TRUE(status.has_value());
    ASSERT_EQ(status, device::CtapDeviceResponseCode::kSuccess);
    ASSERT_EQ(responses.size(), 1u);
  }

  bool Register() {
    BoolCallback register_callback;
    manager_.RegisterIfNeeded(register_callback.callback());
    register_callback.WaitForCallback();
    return std::get<0>(register_callback.result().value());
  }

  void ConfigureVaultResponses() {
    url_loader_factory_.AddResponse(
        std::string(EnclaveManager::recovery_key_store_cert_url_for_testing()),
        std::string(kSampleRecoverableKeyStoreCertXML));
    url_loader_factory_.AddResponse(
        std::string(EnclaveManager::recovery_key_store_sig_url_for_testing()),
        std::string(kSampleRecoverableKeyStoreSigXML));
    url_loader_factory_.AddResponse(
        std::string(EnclaveManager::recovery_key_store_url_for_testing()) +
            "?alt=proto",
        MakeVaultResponse());
  }

  void CorruptDeviceId() {
    webauthn_pb::EnclaveLocalState& state = manager_.local_state_for_testing();
    ASSERT_EQ(state.users().size(), 1u);
    state.mutable_users()->begin()->second.set_device_id("corrupted value");
  }

  base::test::TaskEnvironment task_env_;
  unsigned stored_count_ = 0;
  const TempDir temp_dir_;
  const std::pair<base::Process, uint16_t> process_and_port_;
  const enclave::ScopedEnclaveOverride enclave_override_;
  network::TestURLLoaderFactory url_loader_factory_;
  mojo::Remote<network::mojom::NetworkContext> network_context_;
  std::unique_ptr<network::NetworkService> network_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  std::string gaia_id_;
  std::unique_ptr<FakeSecurityDomainService> security_domain_service_;
  std::unique_ptr<crypto::ScopedMockUnexportableKeyProvider> mock_hw_provider_;
  EnclaveManager manager_;
};

TEST_F(EnclaveManagerTest, TestInfrastructure) {
  // Tests that the enclave starts up.
}

TEST_F(EnclaveManagerTest, Basic) {
  security_domain_service_->pretend_there_are_members();

  ASSERT_FALSE(manager_.is_loaded());
  ASSERT_FALSE(manager_.is_registered());
  ASSERT_FALSE(manager_.is_ready());

  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_FALSE(manager_.is_registered());
  ASSERT_FALSE(manager_.is_ready());

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_idle());
  register_callback.WaitForCallback();
  ASSERT_TRUE(std::get<0>(register_callback.result().value()));
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_FALSE(manager_.is_ready());

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());
  EXPECT_EQ(stored_count_, 1u);

  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*serialized_wrapped_pin=*/std::nullopt, add_callback.callback()));
  ASSERT_FALSE(manager_.is_idle());
  add_callback.WaitForCallback();
  ASSERT_TRUE(std::get<0>(add_callback.result().value()));

  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_FALSE(manager_.has_pending_keys());
  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 0u);

  DoCreate(/*claimed_pin=*/nullptr, /*out_specifics=*/nullptr);
  DoAssertion(GetTestEntity(), /*claimed_pin=*/nullptr);
}

TEST_F(EnclaveManagerTest, SecretsArriveBeforeRegistrationRequested) {
  security_domain_service_->pretend_there_are_members();
  ASSERT_FALSE(manager_.is_registered());

  // If secrets are provided before `RegisterIfNeeded` is called, the state
  // machine should still trigger registration.
  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*serialized_wrapped_pin=*/std::nullopt, add_callback.callback()));
  add_callback.WaitForCallback();

  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(manager_.is_ready());
}

TEST_F(EnclaveManagerTest, SecretsArriveBeforeRegistrationCompleted) {
  security_domain_service_->pretend_there_are_members();
  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_registered());

  // Provide the domain secrets before the registration has completed. The
  // system should still end up in the correct state.
  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*serialized_wrapped_pin=*/std::nullopt, add_callback.callback()));
  add_callback.WaitForCallback();
  register_callback.WaitForCallback();

  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(manager_.is_ready());
}

TEST_F(EnclaveManagerTest, RegistrationFailureAndRetry) {
  const std::string gaia =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;

  // Override the enclave with port=100, which will cause connection failures.
  {
    device::enclave::ScopedEnclaveOverride override(
        TestEnclaveIdentity(/*port=*/100));
    BoolCallback register_callback;
    manager_.RegisterIfNeeded(register_callback.callback());
    register_callback.WaitForCallback();
    ASSERT_FALSE(std::get<0>(register_callback.result().value()));
  }
  ASSERT_FALSE(manager_.is_registered());
  const std::string public_key = manager_.local_state_for_testing()
                                     .users()
                                     .find(gaia)
                                     ->second.hardware_public_key();
  ASSERT_FALSE(public_key.empty());

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  register_callback.WaitForCallback();
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(std::get<0>(register_callback.result().value()));

  // The public key should not have changed because re-registration attempts
  // must try the same public key again in case they actually worked the first
  // time.
  ASSERT_TRUE(public_key == manager_.local_state_for_testing()
                                .users()
                                .find(gaia)
                                ->second.hardware_public_key());
}

TEST_F(EnclaveManagerTest, PrimaryUserChange) {
  const std::string gaia1 =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;

  {
    BoolCallback register_callback;
    manager_.RegisterIfNeeded(register_callback.callback());
    register_callback.WaitForCallback();
  }
  ASSERT_TRUE(manager_.is_registered());
  EXPECT_THAT(GaiaAccountsInState(), testing::UnorderedElementsAre(gaia1));

  identity_test_env_.MakePrimaryAccountAvailable("test2@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  const std::string gaia2 =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  ASSERT_FALSE(manager_.is_registered());
  {
    BoolCallback register_callback;
    manager_.RegisterIfNeeded(register_callback.callback());
    register_callback.WaitForCallback();
  }
  ASSERT_TRUE(manager_.is_registered());
  EXPECT_THAT(GaiaAccountsInState(),
              testing::UnorderedElementsAre(gaia1, gaia2));

  // Remove all accounts from the cookie jar. The primary account should be
  // retained.
  identity_test_env_.SetCookieAccounts({});
  EXPECT_THAT(GaiaAccountsInState(), testing::UnorderedElementsAre(gaia2));

  // When the primary account changes, the second account should be dropped
  // because it was removed from the cookie jar.
  identity_test_env_.MakePrimaryAccountAvailable("test3@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  const std::string gaia3 =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;
  EXPECT_THAT(GaiaAccountsInState(), testing::UnorderedElementsAre(gaia3));
}

TEST_F(EnclaveManagerTest, PrimaryUserChangeDiscardsActions) {
  security_domain_service_->pretend_there_are_members();
  const std::string gaia1 =
      identity_test_env_.identity_manager()
          ->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin)
          .gaia;

  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();

  BoolCallback register_callback1;
  manager_.RegisterIfNeeded(register_callback1.callback());
  BoolCallback register_callback2;
  manager_.RegisterIfNeeded(register_callback2.callback());

  identity_test_env_.MakePrimaryAccountAvailable("test2@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  // `MakePrimaryAccountAvailable` should have canceled any actions.
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_FALSE(manager_.has_pending_keys());
  ASSERT_FALSE(manager_.is_registered());
  ASSERT_FALSE(manager_.is_ready());

  register_callback1.WaitForCallback();
  ASSERT_FALSE(std::get<0>(register_callback1.result().value()));
  register_callback2.WaitForCallback();
  ASSERT_FALSE(std::get<0>(register_callback2.result().value()));
}

TEST_F(EnclaveManagerTest, AddWithExistingPIN) {
  security_domain_service_->pretend_there_are_members();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*serialized_wrapped_pin=*/GetTestWrappedPIN().SerializeAsString(),
      add_callback.callback()));
  add_callback.WaitForCallback();

  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.is_loaded());
  ASSERT_TRUE(manager_.is_registered());
  ASSERT_TRUE(manager_.is_ready());

  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  // The PIN should not have been added to the account. Instead this test is
  // pretending that it was already there.
  EXPECT_EQ(security_domain_service_->num_pin_members(), 0u);
  EXPECT_TRUE(manager_.has_wrapped_pin());
}

TEST_F(EnclaveManagerTest, InvalidWrappedPIN) {
  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);

  BoolCallback add_callback;
  // A wrapped PIN that isn't a valid protobuf should be rejected.
  EXPECT_FALSE(manager_.AddDeviceToAccount("nonsense wrapped PIN",
                                           add_callback.callback()));

  // A valid protobuf, but which fails invariants, should be rejected.
  webauthn_pb::EnclaveLocalState::WrappedPIN wrapped_pin = GetTestWrappedPIN();
  wrapped_pin.set_wrapped_pin("too short");
  EXPECT_FALSE(manager_.AddDeviceToAccount(wrapped_pin.SerializeAsString(),
                                           add_callback.callback()));
}

TEST_F(EnclaveManagerTest, SetupWithPIN) {
  const std::string pin = "123456";
  ConfigureVaultResponses();

  BoolCallback setup_callback;
  manager_.SetupWithPIN(pin, setup_callback.callback());
  setup_callback.WaitForCallback();
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_TRUE(manager_.has_wrapped_pin());
  EXPECT_FALSE(manager_.wrapped_pin_is_arbitrary());

  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 1u);

  std::unique_ptr<device::enclave::ClaimedPIN> claimed_pin =
      EnclaveManager::MakeClaimedPINSlowly(pin, manager_.GetWrappedPIN());
  std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> entity;
  DoCreate(/*claimed_pin=*/nullptr, &entity);
  DoAssertion(std::move(entity), std::move(claimed_pin));
}

TEST_F(EnclaveManagerTest, SetupWithPIN_CertXMLFailure) {
  url_loader_factory_.AddResponse(
      std::string(EnclaveManager::recovery_key_store_cert_url_for_testing()),
      std::string(), net::HTTP_NOT_FOUND);
  url_loader_factory_.AddResponse(
      std::string(EnclaveManager::recovery_key_store_sig_url_for_testing()),
      std::string(kSampleRecoverableKeyStoreSigXML));

  BoolCallback setup_callback;
  manager_.SetupWithPIN("123456", setup_callback.callback());
  // This test primarily shouldn't crash or hang.
  setup_callback.WaitForCallback();
  ASSERT_FALSE(std::get<0>(setup_callback.result().value()));
  ASSERT_FALSE(manager_.is_ready());
}

TEST_F(EnclaveManagerTest, SetupWithPIN_SigXMLFailure) {
  url_loader_factory_.AddResponse(
      std::string(EnclaveManager::recovery_key_store_cert_url_for_testing()),
      std::string(kSampleRecoverableKeyStoreCertXML));
  url_loader_factory_.AddResponse(
      std::string(EnclaveManager::recovery_key_store_sig_url_for_testing()),
      std::string(), net::HTTP_NOT_FOUND);

  BoolCallback setup_callback;
  manager_.SetupWithPIN("123456", setup_callback.callback());
  // This test primarily shouldn't crash or hang.
  setup_callback.WaitForCallback();
  ASSERT_FALSE(std::get<0>(setup_callback.result().value()));
  ASSERT_FALSE(manager_.is_ready());
}

TEST_F(EnclaveManagerTest, AddDeviceAndPINToAccount) {
  security_domain_service_->pretend_there_are_members();
  ConfigureVaultResponses();
  const std::string pin = "pin";

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolCallback add_callback;
  manager_.AddDeviceAndPINToAccount(pin, add_callback.callback());
  add_callback.WaitForCallback();
  ASSERT_TRUE(manager_.is_ready());
  ASSERT_TRUE(manager_.has_wrapped_pin());
  EXPECT_TRUE(manager_.wrapped_pin_is_arbitrary());

  EXPECT_EQ(security_domain_service_->num_physical_members(), 1u);
  EXPECT_EQ(security_domain_service_->num_pin_members(), 1u);

  std::unique_ptr<device::enclave::ClaimedPIN> claimed_pin =
      EnclaveManager::MakeClaimedPINSlowly(pin, manager_.GetWrappedPIN());
  std::unique_ptr<sync_pb::WebauthnCredentialSpecifics> entity;
  DoCreate(/*claimed_pin=*/nullptr, &entity);
  DoAssertion(std::move(entity), std::move(claimed_pin));
}

TEST_F(EnclaveManagerTest, EnclaveForgetsClient_SetupWithPIN) {
  ASSERT_TRUE(Register());
  CorruptDeviceId();
  ConfigureVaultResponses();

  BoolCallback setup_callback;
  manager_.SetupWithPIN("1234", setup_callback.callback());
  setup_callback.WaitForCallback();
  EXPECT_FALSE(std::get<0>(setup_callback.result().value()));
}

TEST_F(EnclaveManagerTest, EnclaveForgetsClient_AddDeviceToAccount) {
  ASSERT_TRUE(Register());
  CorruptDeviceId();
  security_domain_service_->pretend_there_are_members();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*serialized_wrapped_pin=*/GetTestWrappedPIN().SerializeAsString(),
      add_callback.callback()));
  add_callback.WaitForCallback();
  EXPECT_FALSE(std::get<0>(add_callback.result().value()));
}

TEST_F(EnclaveManagerTest, EnclaveForgetsClient_AddDeviceAndPINToAccount) {
  ASSERT_TRUE(Register());
  CorruptDeviceId();
  ConfigureVaultResponses();
  security_domain_service_->pretend_there_are_members();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/417);
  BoolCallback add_callback;
  manager_.AddDeviceAndPINToAccount("1234", add_callback.callback());
  add_callback.WaitForCallback();
  EXPECT_FALSE(std::get<0>(add_callback.result().value()));
}

// UV keys are only supported on Windows at this time.
#if BUILDFLAG(IS_WIN)
#define MAYBE_UserVerifyingKeyAvailable UserVerifyingKeyAvailable
#else
#define MAYBE_UserVerifyingKeyAvailable DISABLED_UserVerifyingKeyAvailable
#endif
TEST_F(EnclaveManagerTest, MAYBE_UserVerifyingKeyAvailable) {
  crypto::ScopedFakeUserVerifyingKeyProvider fake_uv_provider;
  security_domain_service_->pretend_there_are_members();
  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_idle());
  register_callback.WaitForCallback();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*serialized_wrapped_pin=*/std::nullopt, add_callback.callback()));
  ASSERT_FALSE(manager_.is_idle());
  add_callback.WaitForCallback();

  EXPECT_EQ(manager_.uv_key_state(), EnclaveManager::UvKeyState::kUsesSystemUI);
}

// UV keys are only supported on Windows at this time.
#if BUILDFLAG(IS_WIN)
#define MAYBE_UserVerifyingKeyUnavailable UserVerifyingKeyUnavailable
#else
#define MAYBE_UserVerifyingKeyUnavailable DISABLED_UserVerifyingKeyUnavailable
#endif
TEST_F(EnclaveManagerTest, MAYBE_UserVerifyingKeyUnavailable) {
  crypto::ScopedNullUserVerifyingKeyProvider null_uv_provider;
  security_domain_service_->pretend_there_are_members();
  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_idle());
  register_callback.WaitForCallback();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*serialized_wrapped_pin=*/std::nullopt, add_callback.callback()));
  ASSERT_FALSE(manager_.is_idle());
  add_callback.WaitForCallback();
  ASSERT_TRUE(manager_.is_registered());
  EXPECT_EQ(manager_.uv_key_state(), EnclaveManager::UvKeyState::kNone);
}

// UV keys are only supported on Windows at this time.
#if BUILDFLAG(IS_WIN)
#define MAYBE_UserVerifyingKeyLost UserVerifyingKeyLost
#else
#define MAYBE_UserVerifyingKeyLost DISABLED_UserVerifyingKeyLost
#endif
TEST_F(EnclaveManagerTest, MAYBE_UserVerifyingKeyLost) {
  {
    crypto::ScopedFakeUserVerifyingKeyProvider fake_uv_provider;
    security_domain_service_->pretend_there_are_members();
    NoArgCallback loaded_callback;
    manager_.Load(loaded_callback.callback());
    loaded_callback.WaitForCallback();

    BoolCallback register_callback;
    manager_.RegisterIfNeeded(register_callback.callback());
    ASSERT_FALSE(manager_.is_idle());
    register_callback.WaitForCallback();

    std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
    ASSERT_FALSE(manager_.has_pending_keys());
    manager_.StoreKeys(gaia_id_, {std::move(key)},
                       /*last_key_version=*/kSecretVersion);
    ASSERT_TRUE(manager_.is_idle());
    ASSERT_TRUE(manager_.has_pending_keys());

    BoolCallback add_callback;
    ASSERT_TRUE(manager_.AddDeviceToAccount(
        /*serialized_wrapped_pin=*/std::nullopt, add_callback.callback()));
    ASSERT_FALSE(manager_.is_idle());
    add_callback.WaitForCallback();

    ASSERT_EQ(manager_.uv_key_state(),
              EnclaveManager::UvKeyState::kUsesSystemUI);
  }
  manager_.ClearCachedKeysForTesting();
  {
    crypto::ScopedNullUserVerifyingKeyProvider null_uv_provider;
    auto signing_callback = manager_.UserVerifyingKeySigningCallback();
    auto quit_closure = task_env_.QuitClosure();
    std::move(signing_callback)
        .Run({1, 2, 3, 4},
             base::BindLambdaForTesting(
                 [&quit_closure](
                     std::optional<enclave::ClientSignature> signature) {
                   EXPECT_EQ(signature, std::nullopt);
                   quit_closure.Run();
                 }));
    task_env_.RunUntilQuit();
    EXPECT_FALSE(manager_.is_registered());
  }
}

// Tests that rely on `ScopedMockUnexportableKeyProvider` only work on
// platforms where EnclaveManager uses `GetUnexportableKeyProvider`, as opposed
// to `GetSoftwareUnsecureUnexportableKeyProvider`.
#if BUILDFLAG(IS_WIN)
#define MAYBE_HardwareKeyLost HardwareKeyLost
#else
#define MAYBE_HardwareKeyLost DISABLED_HardwareKeyLost
#endif
TEST_F(EnclaveManagerTest, MAYBE_HardwareKeyLost) {
  security_domain_service_->pretend_there_are_members();
  NoArgCallback loaded_callback;
  manager_.Load(loaded_callback.callback());
  loaded_callback.WaitForCallback();

  BoolCallback register_callback;
  manager_.RegisterIfNeeded(register_callback.callback());
  ASSERT_FALSE(manager_.is_idle());
  register_callback.WaitForCallback();

  std::vector<uint8_t> key(kTestKey.begin(), kTestKey.end());
  ASSERT_FALSE(manager_.has_pending_keys());
  manager_.StoreKeys(gaia_id_, {std::move(key)},
                     /*last_key_version=*/kSecretVersion);
  ASSERT_TRUE(manager_.is_idle());
  ASSERT_TRUE(manager_.has_pending_keys());

  BoolCallback add_callback;
  ASSERT_TRUE(manager_.AddDeviceToAccount(
      /*serialized_wrapped_pin=*/std::nullopt, add_callback.callback()));
  ASSERT_FALSE(manager_.is_idle());
  add_callback.WaitForCallback();
  mock_hw_provider_.reset();
  manager_.ClearCachedKeysForTesting();

  crypto::ScopedNullUnexportableKeyProvider null_hw_provider;
  auto signing_callback = manager_.HardwareKeySigningCallback();
  auto quit_closure = task_env_.QuitClosure();
  std::move(signing_callback)
      .Run({1, 2, 3, 4},
           base::BindLambdaForTesting(
               [&quit_closure](
                   std::optional<enclave::ClientSignature> signature) {
                 EXPECT_EQ(signature, std::nullopt);
                 quit_closure.Run();
               }));
  task_env_.RunUntilQuit();
  EXPECT_FALSE(manager_.is_registered());
}

}  // namespace

#endif  // !defined(MEMORY_SANITIZER)
