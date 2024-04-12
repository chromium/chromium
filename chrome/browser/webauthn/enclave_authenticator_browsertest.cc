// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/rand_util.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/webauthn/authenticator_request_dialog_model.h"
#include "chrome/browser/webauthn/chrome_authenticator_request_delegate.h"
#include "chrome/browser/webauthn/enclave_manager.h"
#include "chrome/browser/webauthn/enclave_manager_factory.h"
#include "chrome/browser/webauthn/fake_security_domain_service.h"
#include "chrome/browser/webauthn/gpm_enclave_controller.h"
#include "chrome/browser/webauthn/passkey_model_factory.h"
#include "chrome/browser/webauthn/test_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/service/sync_service_impl.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "components/trusted_vault/proto/recovery_key_store.pb.h"
#include "components/trusted_vault/proto/vault.pb.h"
#include "components/trusted_vault/securebox.h"
#include "components/trusted_vault/trusted_vault_connection.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "crypto/scoped_fake_user_verifying_key_provider.h"
#include "crypto/scoped_mock_unexportable_key_provider.h"
#include "device/fido/enclave/constants.h"
#include "device/fido/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

// These tests are disabled under MSAN. The enclave subprocess is written in
// Rust and FFI from Rust to C++ doesn't work in Chromium at this time
// (crbug.com/1369167).
#if !defined(MEMORY_SANITIZER)

namespace {

constexpr int32_t kSecretVersion = 417;

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

constexpr uint8_t kSecurityDomainSecret[32] = {0};

// Protobuf generated by printing one generated by an enclave using
// `kSecurityDomainSecret`.
constexpr uint8_t kTestProtobuf[] = {
    0x0A, 0x10, 0x8E, 0x48, 0x4B, 0x1C, 0x4F, 0xF9, 0x01, 0x14, 0xEF, 0xEA,
    0xB3, 0x18, 0x40, 0x21, 0xEB, 0xF9, 0x12, 0x10, 0x48, 0x74, 0x02, 0x2C,
    0xC5, 0x85, 0x38, 0xDA, 0x22, 0xD8, 0x8C, 0xAF, 0xD4, 0x05, 0x29, 0x84,
    0x1A, 0x0F, 0x77, 0x77, 0x77, 0x2E, 0x65, 0x78, 0x61, 0x6D, 0x70, 0x6C,
    0x65, 0x2E, 0x63, 0x6F, 0x6D, 0x22, 0x01, 0x00, 0x30, 0xE4, 0xFA, 0x86,
    0x8D, 0xAC, 0x86, 0xDD, 0x17, 0x3A, 0x03, 0x66, 0x6F, 0x6F, 0x42, 0x00,
    0x62, 0xCB, 0x01, 0x30, 0x89, 0x28, 0x56, 0xC4, 0x9C, 0xC4, 0xAD, 0x19,
    0x4D, 0x4B, 0x91, 0x12, 0xD4, 0xA0, 0x05, 0xF0, 0xA4, 0xCA, 0x87, 0x66,
    0x4C, 0x9E, 0x49, 0x58, 0xED, 0x08, 0x92, 0xB9, 0x5C, 0x5C, 0xCD, 0x7D,
    0xA7, 0xD4, 0xEA, 0x54, 0xE9, 0x7E, 0xF2, 0x93, 0xDA, 0x17, 0x43, 0x7F,
    0x41, 0x15, 0x25, 0x94, 0xB8, 0x04, 0x08, 0xAD, 0xE7, 0x67, 0xFA, 0xE2,
    0x38, 0xD3, 0x37, 0xCE, 0x68, 0x1C, 0x2C, 0x82, 0xCA, 0xED, 0x8D, 0x10,
    0x32, 0x31, 0xD9, 0xED, 0x7F, 0x51, 0x74, 0x66, 0x63, 0x14, 0x12, 0xD3,
    0xA1, 0xC0, 0xFE, 0x52, 0xA3, 0x07, 0x01, 0x58, 0xDD, 0x3F, 0xD4, 0x97,
    0xD8, 0xFA, 0x7F, 0x9A, 0xB2, 0xC1, 0x65, 0x36, 0xE2, 0xBE, 0xDF, 0x00,
    0xFB, 0xAC, 0x59, 0xFE, 0x93, 0x25, 0x18, 0xA3, 0x92, 0xBF, 0x06, 0x8E,
    0x0F, 0x2E, 0xD6, 0xE8, 0xFE, 0xCD, 0xE5, 0x76, 0xB8, 0x92, 0x3D, 0xB1,
    0x42, 0xE9, 0xBB, 0x54, 0x36, 0x99, 0x5C, 0x21, 0xB7, 0x63, 0x33, 0x20,
    0x8E, 0x93, 0xAA, 0x00, 0x83, 0xC6, 0xCC, 0x23, 0xAD, 0x63, 0x2B, 0x34,
    0xAA, 0x4F, 0x8E, 0x9B, 0xFA, 0x40, 0x0E, 0xDB, 0x30, 0x37, 0x58, 0xE4,
    0x60, 0xA2, 0xDF, 0x99, 0x85, 0x4B, 0x5C, 0xDD, 0x44, 0x23, 0x12, 0x64,
    0x4C, 0x50, 0x34, 0x9D, 0x24, 0x1B, 0x37, 0x40, 0xC5, 0xB5, 0xA1, 0x5A,
    0x70, 0x33, 0xF7, 0x80, 0x75, 0x1D, 0x22, 0x13, 0x37, 0xCD, 0x1F, 0x24,
    0x40, 0xDA, 0x70, 0xA1, 0x03};

static constexpr char kMakeCredentialUvDiscouraged[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { name: "www.example.com" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    authenticatorSelection: {
      userVerification: 'discouraged',
      requireResidentKey: true,
    },
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kMakeCredentialUvRequired[] = R"((() => {
  return navigator.credentials.create({ publicKey: {
    rp: { name: "" },
    user: { id: new Uint8Array([0]), name: "foo", displayName: "" },
    pubKeyCredParams: [{type: "public-key", alg: -7}],
    challenge: new Uint8Array([0]),
    timeout: 10000,
    authenticatorSelection: {
      requireResidentKey: true,
      userVerification: 'required',
    },
  }}).then(c => window.domAutomationController.send(
              'webauthn: uv=' +
              // This gets the UV bit from the response.
              ((new Uint8Array(c.response.getAuthenticatorData())[32]&4) != 0)),
           e => window.domAutomationController.send('error ' + e));
})())";

static constexpr char kGetAssertionUvDiscouraged[] = R"((() => {
  return navigator.credentials.get({ publicKey: {
    challenge: new Uint8Array([0]),
    timeout: 10000,
    userVerification: 'discouraged',
    allowCredentials: [],
  }}).then(c => window.domAutomationController.send('webauthn: OK'),
           e => window.domAutomationController.send('error ' + e));
})())";

struct TempDir {
 public:
  TempDir() { CHECK(dir_.CreateUniqueTempDir()); }

  base::FilePath GetPath() const { return dir_.GetPath(); }

 private:
  base::ScopedTempDir dir_;
};

class MockTrustedVaultConnection
    : public trusted_vault::TrustedVaultConnection {
 public:
  MockTrustedVaultConnection() = default;
  ~MockTrustedVaultConnection() override = default;
  MOCK_METHOD(
      std::unique_ptr<Request>,
      RegisterAuthenticationFactor,
      (const CoreAccountInfo& account_info,
       const trusted_vault::MemberKeysSource& member_key_source,
       const trusted_vault::SecureBoxPublicKey&
           authentication_factor_public_key,
       trusted_vault::AuthenticationFactorType authentication_factor_type,
       base::OnceCallback<
           void(const trusted_vault::TrustedVaultRegistrationStatus,
                /*key_version=*/int)> callback),
      (override));
  MOCK_METHOD(std::unique_ptr<Request>,
              RegisterDeviceWithoutKeys,
              (const CoreAccountInfo& account_info,
               const trusted_vault::SecureBoxPublicKey& device_public_key,
               base::OnceCallback<
                   void(const trusted_vault::TrustedVaultRegistrationStatus,
                        /*key_version=*/int)> callback),
              (override));
  MOCK_METHOD(std::unique_ptr<Request>,
              DownloadNewKeys,
              (const CoreAccountInfo& account_info,
               const trusted_vault::TrustedVaultKeyAndVersion&
                   last_trusted_vault_key_and_version,
               std::unique_ptr<trusted_vault::SecureBoxKeyPair> device_key_pair,
               base::OnceCallback<
                   void(trusted_vault::TrustedVaultDownloadKeysStatus,
                        const std::vector<std::vector<uint8_t>>& /*keys*/,
                        int /*last_key_version*/)> callback),
              (override));
  MOCK_METHOD(std::unique_ptr<Request>,
              DownloadIsRecoverabilityDegraded,
              (const CoreAccountInfo& account_info,
               base::OnceCallback<
                   void(trusted_vault::TrustedVaultRecoverabilityStatus)>),
              (override));
  MOCK_METHOD(
      std::unique_ptr<Request>,
      DownloadAuthenticationFactorsRegistrationState,
      (const CoreAccountInfo& account_info,
       base::OnceCallback<void(
           trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult)>
           callback),
      (override));
};

class EnclaveAuthenticatorBrowserTest : public SyncTest {
 public:
  class DelegateObserver
      : public ChromeAuthenticatorRequestDelegate::TestObserver {
   public:
    explicit DelegateObserver(EnclaveAuthenticatorBrowserTest* test_instance)
        : test_instance_(test_instance) {
      run_loop_ = std::make_unique<base::RunLoop>();
    }
    virtual ~DelegateObserver() = default;

    void WaitForUI() { run_loop_->Run(); }

    void SetPendingTrustedVaultConnection(
        std::unique_ptr<trusted_vault::TrustedVaultConnection> connection) {
      pending_connection_ = std::move(connection);
    }

    // ChromeAuthenticatorRequestDelegate::TestObserver:
    void Created(ChromeAuthenticatorRequestDelegate* delegate) override {
      test_instance_->UpdateRequestDelegate(delegate);
      if (pending_connection_) {
        delegate->SetTrustedVaultConnectionForTesting(
            std::move(pending_connection_));
      }
    }

    void OnDestroy(ChromeAuthenticatorRequestDelegate* delegate) override {
      test_instance_->UpdateRequestDelegate(nullptr);
    }

    std::vector<std::unique_ptr<device::cablev2::Pairing>>
    GetCablePairingsFromSyncedDevices() override {
      return {};
    }

    void OnTransportAvailabilityEnumerated(
        ChromeAuthenticatorRequestDelegate* delegate,
        device::FidoRequestHandlerBase::TransportAvailabilityInfo* tai)
        override {}

    void UIShown(ChromeAuthenticatorRequestDelegate* delegate) override {
      run_loop_->QuitWhenIdle();
    }

    void CableV2ExtensionSeen(
        base::span<const uint8_t> server_link_data) override {}

    void AccountSelectorShown(
        const std::vector<device::AuthenticatorGetAssertionResponse>& responses)
        override {}

   private:
    raw_ptr<EnclaveAuthenticatorBrowserTest> test_instance_;
    std::unique_ptr<trusted_vault::TrustedVaultConnection> pending_connection_;
    std::unique_ptr<base::RunLoop> run_loop_;
  };

  class ModelObserver : public AuthenticatorRequestDialogModel::Observer {
   public:
    explicit ModelObserver(AuthenticatorRequestDialogModel* model)
        : model_(model) {
      model_->observers.AddObserver(this);
    }

    ~ModelObserver() override {
      if (model_) {
        model_->observers.RemoveObserver(this);
        model_ = nullptr;
      }
    }

    // Call this before the state transition you are looking to observe.
    void SetStepToObserve(AuthenticatorRequestDialogModel::Step step) {
      step_ = step;
      run_loop_ = std::make_unique<base::RunLoop>();
    }

    // This will return after a transition to the state previously specified by
    // `SetStepToObserver`.
    void WaitForStep() {
      ASSERT_TRUE(run_loop_);
      run_loop_->Run();
      CHECK_EQ(step_, model_->step());
      step_ = AuthenticatorRequestDialogModel::Step::kNotStarted;
      run_loop_.reset();
    }

    // AuthenticatorRequestDialogModel::Observer:
    void OnStepTransition() override {
      if (run_loop_ && step_ == model_->step()) {
        run_loop_->QuitWhenIdle();
      }
    }

    void OnModelDestroyed(AuthenticatorRequestDialogModel* model) override {
      model_ = nullptr;
    }

   private:
    raw_ptr<AuthenticatorRequestDialogModel> model_;
    AuthenticatorRequestDialogModel::Step step_ =
        AuthenticatorRequestDialogModel::Step::kNotStarted;
    std::unique_ptr<base::RunLoop> run_loop_;
  };

  EnclaveAuthenticatorBrowserTest()
      : SyncTest(SINGLE_CLIENT),
        process_and_port_(StartWebAuthnEnclave(temp_dir_.GetPath())),
        enclave_override_(
            TestWebAuthnEnclaveIdentity(process_and_port_.second)),
        security_domain_service_(
            FakeSecurityDomainService::New(kSecretVersion)) {
    OSCryptMocker::SetUp();

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

    trusted_vault_pb::Vault vault;
    vault.mutable_vault_parameters()->set_vault_handle("test vault handle");
    url_loader_factory_.AddResponse(
        std::string(EnclaveManager::recovery_key_store_cert_url_for_testing()),
        std::string(kSampleRecoverableKeyStoreCertXML));
    url_loader_factory_.AddResponse(
        std::string(EnclaveManager::recovery_key_store_sig_url_for_testing()),
        std::string(kSampleRecoverableKeyStoreSigXML));
    url_loader_factory_.AddResponse(
        std::string(EnclaveManager::recovery_key_store_url_for_testing()) +
            "?alt=proto",
        vault.SerializeAsString());
  }

  ~EnclaveAuthenticatorBrowserTest() override {
    EnclaveManagerFactory::SetUrlLoaderFactoryForTesting(nullptr);
    CHECK(process_and_port_.first.Terminate(/*exit_code=*/1, /*wait=*/true));
    OSCryptMocker::TearDown();
  }

  EnclaveAuthenticatorBrowserTest(const EnclaveAuthenticatorBrowserTest&) =
      delete;
  EnclaveAuthenticatorBrowserTest& operator=(
      const EnclaveAuthenticatorBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SyncTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
  }

  void SetUp() override {
    ASSERT_TRUE(https_server_.InitializeAndListen());
    EnclaveManagerFactory::SetUrlLoaderFactoryForTesting(
        url_loader_factory_.GetSafeWeakWrapper().get());

    SyncTest::SetUp();
  }

  void SetUpInProcessBrowserTestFixture() override {
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  IdentityTestEnvironmentProfileAdaptor::
                      SetIdentityTestEnvironmentFactoriesOnBrowserContext(
                          context);
                }));
  }

  void SetUpOnMainThread() override {
    SyncTest::SetUpOnMainThread();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    identity_test_env()->SetAutomaticIssueOfAccessTokens(true);

    syncer::SyncServiceImpl* sync_service =
        SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
            browser()->profile());
    sync_service->OverrideNetworkForTest(
        fake_server::CreateFakeServerHttpPostProviderFactory(
            GetFakeServer()->AsWeakPtr()));
    sync_harness_ = SyncServiceImplHarness::Create(
        browser()->profile(), "test@gmail.com", "password",
        SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
    ASSERT_TRUE(sync_harness_->SetupSync());
    sync_service->GetUserSettings()->SetSelectedTypes(
        /*sync_everything=*/false,
        /*types=*/{syncer::UserSelectableType::kPasswords});

    https_server_.ServeFilesFromSourceDirectory(GetChromeTestDataDir());
    https_server_.StartAcceptingConnections();
    host_resolver()->AddRule("*", "127.0.0.1");

    delegate_observer_ = std::make_unique<DelegateObserver>(this);
    ChromeAuthenticatorRequestDelegate::SetGlobalObserverForTesting(
        delegate_observer_.get());

    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL("www.example.com", "/title1.html")));
  }

  void TearDownOnMainThread() override { identity_test_env_adaptor_.reset(); }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  void UpdateRequestDelegate(ChromeAuthenticatorRequestDelegate* delegate) {
    request_delegate_ = delegate;
    if (request_delegate_) {
      model_observer_ = std::make_unique<ModelObserver>(dialog_model());
    }
  }

  ChromeAuthenticatorRequestDelegate* request_delegate() {
    return request_delegate_;
  }

  DelegateObserver* delegate_observer() { return delegate_observer_.get(); }

  AuthenticatorRequestDialogModel* dialog_model() {
    return request_delegate()->dialog_model();
  }

  ModelObserver* model_observer() { return model_observer_.get(); }

  webauthn::PasskeyModel* passkey_model() {
    return PasskeyModelFactory::GetInstance()->GetForProfile(
        browser()->profile());
  }

  void SimulateEnclaveMechanismSelection() {
    ASSERT_TRUE(request_delegate_);
    for (const auto& mechanism :
         request_delegate_->dialog_model()->mechanisms) {
      if (mechanism.type ==
          AuthenticatorRequestDialogModel::Mechanism::Type(
              AuthenticatorRequestDialogModel::Mechanism::Enclave())) {
        mechanism.callback.Run();
        return;
      }
    }
    EXPECT_TRUE(false) << "No Enclave mechanism found";
  }

  void AddTestPasskeyToModel() {
    sync_pb::WebauthnCredentialSpecifics passkey;
    CHECK(passkey.ParseFromArray(kTestProtobuf, sizeof(kTestProtobuf)));
    passkey_model()->AddNewPasskeyForTesting(passkey);
  }

  void SetMockVaultConnectionOnRequestDelegate(
      trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
          result) {
    std::unique_ptr<testing::NiceMock<MockTrustedVaultConnection>> connection =
        std::make_unique<testing::NiceMock<MockTrustedVaultConnection>>();
    EXPECT_CALL(*connection, DownloadAuthenticationFactorsRegistrationState(
                                 testing::_, testing::_))
        .WillOnce(
            [result = std::move(result)](
                const CoreAccountInfo&,
                base::OnceCallback<void(
                    trusted_vault::
                        DownloadAuthenticationFactorsRegistrationStateResult)>
                    callback) mutable {
              std::move(callback).Run(std::move(result));
              return std::make_unique<
                  trusted_vault::TrustedVaultConnection::Request>();
            });
    // If the delegate hasn't been created yet, the mock will be assigned upon
    // creation.
    if (request_delegate_) {
      request_delegate_->SetTrustedVaultConnectionForTesting(
          std::move(connection));
    } else {
      delegate_observer_->SetPendingTrustedVaultConnection(
          std::move(connection));
    }
  }

  void EnableUVKeySupport() {
    fake_uv_provider_.emplace<crypto::ScopedFakeUserVerifyingKeyProvider>();
  }

 protected:
  net::EmbeddedTestServer https_server_{net::EmbeddedTestServer::TYPE_HTTPS};
  const TempDir temp_dir_;
  base::CallbackListSubscription subscription_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  std::unique_ptr<SyncServiceImplHarness> sync_harness_;
  const std::pair<base::Process, uint16_t> process_and_port_;
  const device::enclave::ScopedEnclaveOverride enclave_override_;
  std::unique_ptr<FakeSecurityDomainService> security_domain_service_;
  crypto::ScopedMockUnexportableKeyProvider mock_hw_provider_;
  network::TestURLLoaderFactory url_loader_factory_;
  std::unique_ptr<DelegateObserver> delegate_observer_;
  std::unique_ptr<ModelObserver> model_observer_;
  raw_ptr<ChromeAuthenticatorRequestDelegate> request_delegate_;
  absl::variant<crypto::ScopedNullUserVerifyingKeyProvider,
                crypto::ScopedFakeUserVerifyingKeyProvider>
      fake_uv_provider_;
  const base::test::ScopedFeatureList scoped_feature_list_{
      device::kWebAuthnEnclaveAuthenticator};
};

}  // namespace

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       RegisterDeviceWithGpmPin_MakeCredential_Success) {
  /* Test script:
   *  - Prerequisites:
   *       Enclave not registered
   *       No existing security domain secrets
   *       Existing user account with password sync enabled
   *       Platform UV unavailable
   * 1. Modal MakeCredential request invoked by RP
   * 2. Mechanism selection appears; test chooses enclave credential
   * 3. UI for onboarding appears; test accepts it
   * 4. Test selects a GPM PIN
   * 5. Device registration with enclave succeeds
   * 6. MakeCredential succeeds
   */
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kEmpty);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogController::Step::kGPMOnboarding);
  SimulateEnclaveMechanismSelection();
  model_observer()->WaitForStep();

  dialog_model()->OnGPMOnboardingAccepted();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       RegisterDeviceWithGpmPin_MakeCredentialWithUV_Success) {
  /* Test script:
   *  - Prerequisites:
   *       Enclave not registered
   *       No existing security domain secrets
   *       Existing user account with password sync enabled
   *       Platform UV unavailable
   * 1. Modal MakeCredential request invoked by RP, requires UV.
   * 2. Mechanism selection appears; test chooses enclave.
   * 3. UI for onboarding appears; test accepts it
   * 4. Test selects a GPM PIN
   * 5. Device registration with enclave succeeds
   * 6. MakeCredential succeeds
   *
   * Notably, user verification is asserted without a second GPM PIN prompt.
   */
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kEmpty;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kEmpty);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogController::Step::kGPMOnboarding);
  SimulateEnclaveMechanismSelection();
  model_observer()->WaitForStep();

  dialog_model()->OnGPMOnboardingAccepted();
  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kGPMCreatePin);
  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       MakeCredential_RecoverWithGPMPIN_Success) {
  /* Test script:
   *  - Prerequisites:
   *       Enclave not registered
   *       Security domain exists with GPM PIN.
   *       Existing user account with password sync enabled
   *       Platform UV unavailable
   * 1. Modal MakeCredential request invoked by RP, requires UV.
   * 2. Mechanism selection appears; test chooses enclave.
   * 3. UI for onboarding appears; test accepts it
   * 4. Test simiulates MagicArch
   * 5. Test selects a GPM PIN
   * 6. Device registration with enclave succeeds
   * 7. MakeCredential succeeds
   *
   * Notably, user verification is asserted without a second GPM PIN prompt.
   */
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  registration_state_result.gpm_pin_metadata = trusted_vault::GpmPinMetadata(
      "public key", EnclaveManager::MakeWrappedPINForTesting(
                        kSecurityDomainSecret, "123456"));
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  security_domain_service_->pretend_there_are_members();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogController::Step::kTrustThisComputerCreation);
  SimulateEnclaveMechanismSelection();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogController::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  EnclaveManagerFactory::GetForProfile(browser()->profile())
      ->StoreKeys("gaia_id_for_test_gmail.com",
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       MakeCredential_RecoverWithLSKF_Success) {
  /* Test script:
   *  - Prerequisites:
   *       Enclave not registered
   *       Security domain exists with GPM PIN.
   *       Existing user account with password sync enabled
   *       Platform UV unavailable
   * 1. Modal MakeCredential request invoked by RP, requires UV.
   * 2. Mechanism selection appears; test chooses enclave.
   * 3. UI for onboarding appears; test accepts it
   * 4. Test simiulates MagicArch
   * 5. Test selects a GPM PIN
   * 6. Device registration with enclave succeeds
   * 7. MakeCredential succeeds
   *
   * Notably, user verification is asserted without a second GPM PIN prompt.
   */
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));

  security_domain_service_->pretend_there_are_members();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kMakeCredentialUvRequired);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kMechanismSelection);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogController::Step::kTrustThisComputerCreation);
  SimulateEnclaveMechanismSelection();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogController::Step::kRecoverSecurityDomain);
  dialog_model()->OnTrustThisComputer();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogController::Step::kGPMCreatePin);
  EnclaveManagerFactory::GetForProfile(browser()->profile())
      ->StoreKeys("gaia_id_for_test_gmail.com",
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: uv=true\"");
}

IN_PROC_BROWSER_TEST_F(EnclaveAuthenticatorBrowserTest,
                       RecoverWithLSKF_GetAssertion_Success) {
  /* Test script:
   *  - Prerequisites:
   *       Enclave not registered
   *       Trusted vault state is recoverable
   *       Existing user account with password sync enabled
   *       Platform UV unavailable
   *       Synced passkey for the RP available
   * 1. Modal GetAssertion request invoked by RP
   * 2. Priority mechanism selection for synced passkey appears, test confirms
   * 3. Window to recover security domain appears, test simulates reauth
   * 4. Test selects a GPM PIN
   * 5. Device registration with enclave succeeds
   * 6. GetAssertion succeeds
   */
  trusted_vault::DownloadAuthenticationFactorsRegistrationStateResult
      registration_state_result;
  registration_state_result.state = trusted_vault::
      DownloadAuthenticationFactorsRegistrationStateResult::State::kRecoverable;
  registration_state_result.key_version = kSecretVersion;
  SetMockVaultConnectionOnRequestDelegate(std::move(registration_state_result));
  security_domain_service_->pretend_there_are_members();
  AddTestPasskeyToModel();

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::DOMMessageQueue message_queue(web_contents);
  content::ExecuteScriptAsync(web_contents, kGetAssertionUvDiscouraged);
  delegate_observer()->WaitForUI();

  EXPECT_EQ(dialog_model()->step(),
            AuthenticatorRequestDialogModel::Step::kSelectPriorityMechanism);
  EXPECT_EQ(request_delegate()
                ->enclave_controller_for_testing()
                ->account_state_for_testing(),
            GPMEnclaveController::AccountState::kRecoverable);
  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogController::Step::kRecoverSecurityDomain);
  dialog_model()->OnUserConfirmedPriorityMechanism();
  model_observer()->WaitForStep();

  model_observer()->SetStepToObserve(
      AuthenticatorRequestDialogController::Step::kGPMCreatePin);
  EnclaveManagerFactory::GetForProfile(browser()->profile())
      ->StoreKeys("gaia_id_for_test_gmail.com",
                  {std::vector<uint8_t>(std::begin(kSecurityDomainSecret),
                                        std::end(kSecurityDomainSecret))},
                  kSecretVersion);
  model_observer()->WaitForStep();

  dialog_model()->OnGPMPinEntered(u"123456");

  std::string script_result;
  ASSERT_TRUE(message_queue.WaitForMessage(&script_result));
  EXPECT_EQ(script_result, "\"webauthn: OK\"");
}

#endif  // !defined(MEMORY_SANITIZER)
