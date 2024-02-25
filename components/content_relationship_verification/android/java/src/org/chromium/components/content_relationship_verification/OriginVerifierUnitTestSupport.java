// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.components.content_relationship_verification;

import android.content.pm.PackageInfo;
import android.content.pm.Signature;
import android.util.Base64;

import org.robolectric.shadows.ShadowPackageManager;

import org.chromium.base.PackageUtils;

/** Methods that make it easier to unit test functionality relying on {@link OriginVerifier}. */
public class OriginVerifierUnitTestSupport {
    // A valid Android package signature base 64 encoded.
    private static final String PACKAGE_SIGNATURE_BASE64 =
            "MIIFazCCA1OgAwIBAgIUAJxfGUKjrAHbXd8As2Z+5bQH+c4wDQYJKoZIhvcNAQEL"
                    + "BQAwRTELMAkGA1UEBhMCQVUxEzARBgNVBAgMClNvbWUtU3RhdGUxITAfBgNVBAoM"
                    + "GEludGVybmV0IFdpZGdpdHMgUHR5IEx0ZDAeFw0yMjEwMTEwMjE3NTJaFw0yNDEw"
                    + "MTAwMjE3NTJaMEUxCzAJBgNVBAYTAkFVMRMwEQYDVQQIDApTb21lLVN0YXRlMSEw"
                    + "HwYDVQQKDBhJbnRlcm5ldCBXaWRnaXRzIFB0eSBMdGQwggIiMA0GCSqGSIb3DQEB"
                    + "AQUAA4ICDwAwggIKAoICAQCyAAqRRYrQuMOiSuptVeNYWXZo0MkQqOGQ/vf8jdWo"
                    + "0vNLp0bxeCxGGKisDrEiyFp0HklsKd71Rc0rXfp1VLa3jwoOC7OdxXr4ht1psKbT"
                    + "VxjgjXxNg8BnX3lxmuS+S6ZOWtUxuSpwY/kJxPnYANhviZJ2QEp6LOVLqRHWVXZS"
                    + "WKMHzH5q3jFK/r82g1UVUuf3WhVKaJdcoS4/nWQY7KzH1Z+41X2Q58cuUFh9ShQI"
                    + "jM1kwb8qXslWxU3v2d3kC3kOkYLasYt/LC5ZMiSoDZjPN/NK6YxwjCyXRaGCYcGQ"
                    + "oce7hWK8OqXm13CdRFNqgCppbRt7Hf+qZwCPDr4iBINk6MBhGb+n2IZy0U+7r1lK"
                    + "JlDuE8B4J2PdlWQsa7clTioCK0fWLaqRHrmp/gScCBVBrQuAKnL7+gbbS9wsVkZI"
                    + "mwqjXGQUpQcTaVkbE6xplbcBfL+Oezhkp/uRWyUMx3TWWyvkdqHYqv6iSaK9fb/9"
                    + "gv7eCr4gI0zwyNA0/aCCv8E7lfjQ3joz9hAKJY2aLzREWBb/CXHCepNxsttWC/1h"
                    + "6A6ZfxMfjk98zJAQbvHqPu6NDcUsQgDovIBh24gz9pJvtAtfRncvvSaEnNVjlNf/"
                    + "W7pbo0miouZ/iubgFPa7d/TaW88pesR3mp4cvkQmo3CTjNaiexUZ93+GrJE3ckEX"
                    + "CQIDAQABo1MwUTAdBgNVHQ4EFgQU6PDLlJHQa6Q4jAjOhLVLKOzcgq0wHwYDVR0j"
                    + "BBgwFoAU6PDLlJHQa6Q4jAjOhLVLKOzcgq0wDwYDVR0TAQH/BAUwAwEB/zANBgkq"
                    + "hkiG9w0BAQsFAAOCAgEAgVtV1CVKD6bFrMCovdZ5saIb4+4kvuOamfh3nRrvDxBh"
                    + "asL6ddFiV7f+BVoR2qL3p17YvyE9C8UftyXI18eTxeVTtcS5ppNn92356QW0JVgU"
                    + "L3TeU0k6Im2FmY66SBhmgnkjVyHkKa0EMzs726Ir8WFM2PJM/wYzB/Tz9GIo/eJK"
                    + "rjQ/0TogBIzVwBqnQ8jRPeKKm4hja0J7jghdXNDLm6p2Wyyv5PoFMgiKWJ6HLplo"
                    + "s6ep+5xlDxr6tD0ISBLVk1AjdBouamcw7wkqro3Pmr4Ra1FnZfIsLXKtyE4nsSWT"
                    + "D75DjGU+2IF5QOpHCxdttR8jdtK/knuRZp5hMlHnhKn93l35hZvNpT36jbJIPHEP"
                    + "qBnkOnkckeq13yrsNRhl4xLhsiBHJKvVeTiSpEH5o2Q85CjAotno9DC5BGivJ+VG"
                    + "GUZfhmJZFKw35o5YmInt7Z+Ph9hqiJyz+NO4jNDeYgJV01UgiL2NtbvLuLhbLfUp"
                    + "ai22t7riV325JRLy0V8R2DVquRqQpim/nIcw4V/lwVQhq+vPuhCjy/C7Ack5O/rJ"
                    + "L2ItOiQIVoaad++ZYc/ENht/jxYc+RC0JfgRre0msKOxkKra6604U4eTFMLGib8m"
                    + "Ccj/TMwWCX8OS7DttHXSHMBi+3f0PzovvT/uvuIMzopyJtr4bNm1CYBVYJsKYtk=";

    /**
     * Registers the given package with Robolectric's ShadowPackageManager and provides it with a
     * valid signature, so calls to
     * {@link PackageUtils#getCertificateSHA256FingerprintForPackage} will not
     * crash.
     */
    public static void registerPackageWithSignature(
            ShadowPackageManager shadowPackageManager, String packageName, int uid) {
        PackageInfo info = new PackageInfo();
        info.signatures =
                new Signature[] {new Signature(Base64.decode(PACKAGE_SIGNATURE_BASE64, 0))};
        info.packageName = packageName;
        shadowPackageManager.addPackage(info);
        shadowPackageManager.setPackagesForUid(uid, packageName);
    }
}
