// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/webid/account_selection_view_android.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/android/jni_string.h"
#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "content/public/browser/webid/identity_request_dialog_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/webid/federated_request.mojom.h"
#include "third_party/jni_zero/jni_zero.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace {

IdentityProviderDataPtr CreateIdentityProviderData(
    const std::string& idp_for_display) {
  return base::MakeRefCounted<content::IdentityProviderData>(
      idp_for_display, content::IdentityProviderMetadata(),
      content::ClientMetadata(GURL(), GURL(), GURL(), gfx::Image()),
      blink::mojom::RpContext::kSignIn, /*format=*/std::nullopt,
      std::vector<content::IdentityRequestDialogDisclosureField>(),
      /*has_login_status_mismatch=*/false);
}

}  // namespace

TEST(AccountSelectionViewAndroidTest,
     ConvertToJavaIdentityProvidersListPreservesRequestedOrder) {
  IdentityProviderDataPtr first_idp = CreateIdentityProviderData("idp1.com");
  IdentityProviderDataPtr second_idp = CreateIdentityProviderData("idp2.com");
  // Ensure `first_idp` has the higher raw pointer so `base::flat_map` stores
  // `second_idp` first, opposite to the requested `identity_providers` order.
  if (first_idp.get() < second_idp.get()) {
    std::swap(first_idp, second_idp);
  }
  ASSERT_GT(first_idp.get(), second_idp.get());

  JNIEnv* env = jni_zero::AttachCurrentThread();
  jni_zero::ScopedJavaLocalRef<jobject> first_java_idp(
      base::android::ConvertUTF8ToJavaString(env, "first_idp"));
  jni_zero::ScopedJavaLocalRef<jobject> second_java_idp(
      base::android::ConvertUTF8ToJavaString(env, "second_idp"));

  base::flat_map<IdentityProviderDataPtr, jni_zero::ScopedJavaLocalRef<jobject>>
      identity_providers_map;
  identity_providers_map[first_idp] = first_java_idp;
  identity_providers_map[second_idp] = second_java_idp;

  std::vector<jni_zero::ScopedJavaLocalRef<jobject>> result =
      AccountSelectionViewAndroid::ConvertToJavaIdentityProvidersListForTesting(
          {first_idp, second_idp}, identity_providers_map);

  ASSERT_EQ(result.size(), 2u);
  EXPECT_TRUE(env->IsSameObject(result[0].obj(), first_java_idp.obj()));
  EXPECT_TRUE(env->IsSameObject(result[1].obj(), second_java_idp.obj()));
}
