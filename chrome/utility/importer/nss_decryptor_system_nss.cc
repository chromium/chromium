// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/utility/importer/nss_decryptor_system_nss.h"

#include <pk11pub.h>
#include <pk11sdr.h>
#include <stdint.h>
#include <string.h>

#include "base/files/file_path.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "crypto/nss_util.h"

NSSDecryptor::NSSDecryptor() : is_nss_initialized_(false), db_slot_(nullptr) {}
NSSDecryptor::~NSSDecryptor() {
  if (db_slot_) {
    // Deliberately leave the user db open, just in case we need to open more
    // than one, because there's an NSS bug with reopening user dbs.
    // https://bugzilla.mozilla.org/show_bug.cgi?id=506140
    // SECMOD_CloseUserDB(db_slot_);
    PK11_FreeSlot(db_slot_);
  }
}

bool NSSDecryptor::Init(const base::FilePath& dll_path,
                        const base::FilePath& db_path) {
  crypto::EnsureNSSInit();
  is_nss_initialized_ = true;
  const std::string modspec =
      base::StringPrintf(
          "configDir='%s' tokenDescription='Firefox NSS database' "
          "flags=readOnly",
          db_path.value().c_str());
  db_slot_ = SECMOD_OpenUserDB(modspec.c_str());
  return !!db_slot_;
}

// This method is based on some NSS code in
//   security/nss/lib/pk11wrap/pk11sdr.c, CVS revision 1.22
// This code is copied because the implementation assumes the use of the
// internal key slot for decryption, but we need to use another slot.
// The license block is:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Netscape security libraries.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1994-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   thayes@netscape.com
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Data structure and template for encoding the result of an SDR operation
 *  This is temporary.  It should include the algorithm ID of the encryption
 *  mechanism
 */
struct SDRResult
{
  SECItem keyid;
  SECAlgorithmID alg;
  SECItem data;
};
typedef struct SDRResult SDRResult;

static SEC_ASN1Template g_template[] = {
    {SEC_ASN1_SEQUENCE, 0, nullptr, sizeof(SDRResult)},
    {SEC_ASN1_OCTET_STRING, offsetof(SDRResult, keyid)},
    {SEC_ASN1_INLINE | SEC_ASN1_XTRN, offsetof(SDRResult, alg),
     SEC_ASN1_SUB(SECOID_AlgorithmIDTemplate)},
    {SEC_ASN1_OCTET_STRING, offsetof(SDRResult, data)},
    {0}};

static SECStatus
unpadBlock(SECItem *data, int blockSize, SECItem *result)
{
  SECStatus rv = SECSuccess;
  int padLength;
  int i;

  result->data = nullptr;
  result->len = 0;

  /* Remove the padding from the end if the input data */
  if (data->len == 0 || data->len % blockSize  != 0) {
    rv = SECFailure;
    goto loser;
  }

  padLength = data->data[data->len-1];
  if (padLength > blockSize) { rv = SECFailure; goto loser; }

  /* verify padding */
  for (i = data->len - padLength; static_cast<uint32_t>(i) < data->len; i++) {
    if (data->data[i] != padLength) {
        rv = SECFailure;
        goto loser;
    }
  }

  result->len = data->len - padLength;
  result->data = (unsigned char *)PORT_Alloc(result->len);
  if (!result->data) { rv = SECFailure; goto loser; }

  PORT_Memcpy(result->data, data->data, result->len);

  if (padLength < 2) {
    return SECWouldBlock;
  }

loser:
  return rv;
}

/* decrypt a block */
static SECStatus
pk11Decrypt(PK11SlotInfo *slot, PLArenaPool *arena,
            CK_MECHANISM_TYPE type, PK11SymKey *key,
            SECItem *params, SECItem *in, SECItem *result)
{
  PK11Context* ctx = nullptr;
  SECItem paddedResult;
  SECStatus rv;

  paddedResult.len = 0;
  paddedResult.data = nullptr;

  ctx = PK11_CreateContextBySymKey(type, CKA_DECRYPT, key, params);
  if (!ctx) { rv = SECFailure; goto loser; }

  paddedResult.len = in->len;
  paddedResult.data = static_cast<unsigned char*>(
      PORT_ArenaAlloc(arena, paddedResult.len));

  rv = PK11_CipherOp(ctx, paddedResult.data,
                        (int*)&paddedResult.len, paddedResult.len,
                        in->data, in->len);
  if (rv != SECSuccess) goto loser;

  PK11_Finalize(ctx);

  /* Remove the padding */
  rv = unpadBlock(&paddedResult, PK11_GetBlockSize(type, nullptr), result);
  if (rv) goto loser;

loser:
  if (ctx) PK11_DestroyContext(ctx, PR_TRUE);
  return rv;
}

SECStatus NSSDecryptor::PK11SDR_DecryptWithSlot(
    PK11SlotInfo* slot, SECItem* data, SECItem* result, void* cx) const {
  SECStatus rv = SECSuccess;
  PK11SymKey* key = nullptr;
  CK_MECHANISM_TYPE type;
  SDRResult sdrResult;
  SECItem* params = nullptr;
  SECItem possibleResult = {siBuffer, nullptr, 0};
  PLArenaPool* arena = nullptr;

  arena = PORT_NewArena(SEC_ASN1_DEFAULT_ARENA_SIZE);
  if (!arena) { rv = SECFailure; goto loser; }

  /* Decode the incoming data */
  memset(&sdrResult, 0, sizeof sdrResult);
  rv = SEC_QuickDERDecodeItem(arena, &sdrResult, g_template, data);
  if (rv != SECSuccess) goto loser;  /* Invalid format */

  /* Get the parameter values from the data */
  params = PK11_ParamFromAlgid(&sdrResult.alg);
  if (!params) { rv = SECFailure; goto loser; }

  /* Use triple-DES (Should look up the algorithm) */
  type = CKM_DES3_CBC;
  key = PK11_FindFixedKey(slot, type, &sdrResult.keyid, cx);
  if (!key) {
    rv = SECFailure;
  } else {
    rv = pk11Decrypt(slot, arena, type, key, params,
                     &sdrResult.data, result);
  }

  /*
   * if the pad value was too small (1 or 2), then it's statistically
   * 'likely' that (1 in 256) that we may not have the correct key.
   * Check the other keys for a better match. If we find none, use
   * this result.
   */
  if (rv == SECWouldBlock)
    possibleResult = *result;

  /*
   * handle the case where your key indicies may have been broken
   */
  if (rv != SECSuccess) {
    PK11SymKey* keyList = PK11_ListFixedKeysInSlot(slot, nullptr, cx);
    PK11SymKey* testKey = nullptr;
    PK11SymKey* nextKey = nullptr;

    for (testKey = keyList; testKey;
         testKey = PK11_GetNextSymKey(testKey)) {
      rv = pk11Decrypt(slot, arena, type, testKey, params,
                       &sdrResult.data, result);
      if (rv == SECSuccess)
        break;

      /* found a close match. If it's our first remember it */
      if (rv == SECWouldBlock) {
        if (possibleResult.data) {
          /* this is unlikely but possible. If we hit this condition,
           * we have no way of knowing which possibility to prefer.
           * in this case we just match the key the application
           * thought was the right one */
          SECITEM_ZfreeItem(result, PR_FALSE);
        } else {
          possibleResult = *result;
        }
      }
    }

    /* free the list */
    for (testKey = keyList; testKey; testKey = nextKey) {
         nextKey = PK11_GetNextSymKey(testKey);
      PK11_FreeSymKey(testKey);
    }
  }

  /* we didn't find a better key, use the one with a small pad value */
  if ((rv != SECSuccess) && (possibleResult.data)) {
    *result = possibleResult;
    possibleResult.data = nullptr;
    rv = SECSuccess;
  }

 loser:
  if (arena) PORT_FreeArena(arena, PR_TRUE);
  if (key) PK11_FreeSymKey(key);
  if (params) SECITEM_ZfreeItem(params, PR_TRUE);
  if (possibleResult.data) SECITEM_ZfreeItem(&possibleResult, PR_FALSE);

  return rv;
}
