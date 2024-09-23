// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/api/platform_keys_natives.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/values.h"
#include "extensions/renderer/script_context.h"
#include "gin/data_object_builder.h"
#include "third_party/blink/public/platform/web_crypto_algorithm.h"
#include "third_party/blink/public/platform/web_crypto_algorithm_params.h"
#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/public/platform/web_vector.h"
#include "third_party/blink/public/web/web_crypto_normalize.h"

namespace extensions {

namespace {

bool StringToWebCryptoOperation(const std::string& str,
                                blink::WebCryptoOperation* op) {
  if (str == "GenerateKey") {
    *op = blink::kWebCryptoOperationGenerateKey;
    return true;
  }
  if (str == "ImportKey") {
    *op = blink::kWebCryptoOperationImportKey;
    return true;
  }
  if (str == "Sign") {
    *op = blink::kWebCryptoOperationSign;
    return true;
  }
  if (str == "Verify") {
    *op = blink::kWebCryptoOperationVerify;
    return true;
  }
  return false;
}

v8::Local<v8::Object> WebCryptoAlgorithmToV8Value(
    const blink::WebCryptoAlgorithm& algorithm,
    v8::Local<v8::Context> context) {
  DCHECK(!algorithm.IsNull());
  v8::Context::Scope scope(context);
  v8::Isolate* isolate = context->GetIsolate();

  const blink::WebCryptoAlgorithmInfo* info =
      blink::WebCryptoAlgorithm::LookupAlgorithmInfo(algorithm.Id());
  gin::DataObjectBuilder builder(isolate);
  builder.Set("name", std::string_view(info->name));

  const blink::WebCryptoAlgorithm* hash = nullptr;

  switch (algorithm.Id()) {
    case blink::kWebCryptoAlgorithmIdRsaSsaPkcs1v1_5: {
      const blink::WebCryptoRsaHashedKeyGenParams* rsa_hashed_key_gen =
          algorithm.RsaHashedKeyGenParams();
      if (rsa_hashed_key_gen) {
        builder.Set("modulusLength", rsa_hashed_key_gen->ModulusLengthBits());

        const blink::WebVector<unsigned char>& public_exponent =
            rsa_hashed_key_gen->PublicExponent();
        v8::Local<v8::ArrayBuffer> buffer =
            v8::ArrayBuffer::New(isolate, public_exponent.size());
        memcpy(buffer->GetBackingStore()->Data(), public_exponent.data(),
               public_exponent.size());
        builder.Set("publicExponent", buffer);

        hash = &rsa_hashed_key_gen->GetHash();
        DCHECK(!hash->IsNull());
      }
      const blink::WebCryptoRsaHashedImportParams* rsa_hashed_import =
          algorithm.RsaHashedImportParams();
      if (rsa_hashed_import) {
        hash = &rsa_hashed_import->GetHash();
        DCHECK(!hash->IsNull());
      }
      break;
    }
    case blink::kWebCryptoAlgorithmIdEcdsa: {
      const blink::WebCryptoEcKeyGenParams* ec_key_gen =
          algorithm.EcKeyGenParams();
      if (ec_key_gen) {
        std::string_view named_curve;
        switch (ec_key_gen->NamedCurve()) {
          case blink::kWebCryptoNamedCurveP256:
            named_curve = "P-256";
            break;
          case blink::kWebCryptoNamedCurveP384:
            named_curve = "P-384";
            break;
          case blink::kWebCryptoNamedCurveP521:
            named_curve = "P-521";
            break;
        }
        DCHECK(!named_curve.empty());
        builder.Set("namedCurve", named_curve);
      }

      const blink::WebCryptoEcdsaParams* ecdsa = algorithm.EcdsaParams();
      if (ecdsa) {
        hash = &ecdsa->GetHash();
        DCHECK(!hash->IsNull());
      }
      break;
    }
    default: {
      break;
    }
  }

  if (hash) {
    const blink::WebCryptoAlgorithmInfo* hash_info =
        blink::WebCryptoAlgorithm::LookupAlgorithmInfo(hash->Id());

    builder.Set("hash", gin::DataObjectBuilder(isolate)
                            .Set("name", std::string_view(hash_info->name))
                            .Build());
  }
  // Otherwise, |algorithm| is missing support here or no parameters were
  // required.
  return builder.Build();
}

}  // namespace

PlatformKeysNatives::PlatformKeysNatives(ScriptContext* context)
    : ObjectBackedNativeHandler(context) {}

void PlatformKeysNatives::AddRoutes() {
  RouteHandlerFunction(
      "NormalizeAlgorithm",
      base::BindRepeating(&PlatformKeysNatives::NormalizeAlgorithm,
                          base::Unretained(this)));
}

void PlatformKeysNatives::NormalizeAlgorithm(
    const v8::FunctionCallbackInfo<v8::Value>& call_info) {
  DCHECK_EQ(call_info.Length(), 2);
  DCHECK(call_info[0]->IsObject());
  DCHECK(call_info[1]->IsString());

  blink::WebCryptoOperation operation;
  if (!StringToWebCryptoOperation(
          *v8::String::Utf8Value(call_info.GetIsolate(), call_info[1]),
          &operation)) {
    return;
  }

  blink::WebCryptoAlgorithm algorithm =
      blink::NormalizeCryptoAlgorithm(v8::Local<v8::Object>::Cast(call_info[0]),
                                      operation, call_info.GetIsolate());

  if (algorithm.IsNull()) {
    return;
  }

  call_info.GetReturnValue().Set(
      WebCryptoAlgorithmToV8Value(algorithm, context()->v8_context()));
}

}  // namespace extensions
