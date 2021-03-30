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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ian McGreer <mcgreer@netscape.com>
 *   Javier Delgadillo <javi@netscape.com>
 *   John Gardiner Myers <jgmyers@speakeasy.net>
 *   Martin v. Loewis <martin@v.loewis.de>
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

#include "chrome/third_party/mozilla_security_manager/nsNSSCertHelper.h"

#include <certdb.h>
#include <keyhi.h>
#include <prprf.h>
#include <stddef.h>
#include <unicode/uidna.h>

#include "base/i18n/number_formatting.h"
#include "base/lazy_instance.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/common/net/x509_certificate_model_nss.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "crypto/scoped_nss_types.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

std::string BMPtoUTF8(PRArenaPool* arena, unsigned char* data,
                      unsigned int len) {
  if (len % 2 != 0)
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);

  unsigned int utf8_val_len = len * 3 + 1;
  std::vector<unsigned char> utf8_val(utf8_val_len);
  if (!PORT_UCS2_UTF8Conversion(PR_FALSE, data, len,
                                &utf8_val.front(), utf8_val_len, &utf8_val_len))
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);
  return std::string(reinterpret_cast<char*>(&utf8_val.front()), utf8_val_len);
}

SECOidTag RegisterDynamicOid(const char* oid_string) {
  SECOidTag rv = SEC_OID_UNKNOWN;
  unsigned char buffer[1024];
  SECOidData od;
  od.oid.type = siDEROID;
  od.oid.data = buffer;
  od.oid.len = sizeof(buffer);

  if (SEC_StringToOID(NULL, &od.oid, oid_string, 0) == SECSuccess) {
    od.offset = SEC_OID_UNKNOWN;
    od.mechanism = CKM_INVALID_MECHANISM;
    od.supportedExtension = INVALID_CERT_EXTENSION;
    od.desc = oid_string;

    rv = SECOID_AddEntry(&od);
  }
  DCHECK_NE(rv, SEC_OID_UNKNOWN) << oid_string;
  return rv;
}

// Format a SECItem as a space separated string, with 16 bytes on each line.
std::string ProcessRawBytes(SECItem* data) {
  return x509_certificate_model::ProcessRawBytes(data->data, data->len);
}

SECOidTag ms_cert_ext_certtype = SEC_OID_UNKNOWN;
SECOidTag ms_certsrv_ca_version = SEC_OID_UNKNOWN;
SECOidTag ms_nt_principal_name = SEC_OID_UNKNOWN;
SECOidTag ms_ntds_replication = SEC_OID_UNKNOWN;
SECOidTag eku_ms_individual_code_signing = SEC_OID_UNKNOWN;
SECOidTag eku_ms_commercial_code_signing = SEC_OID_UNKNOWN;
SECOidTag eku_ms_trust_list_signing = SEC_OID_UNKNOWN;
SECOidTag eku_ms_time_stamping = SEC_OID_UNKNOWN;
SECOidTag eku_ms_server_gated_crypto = SEC_OID_UNKNOWN;
SECOidTag eku_ms_encrypting_file_system = SEC_OID_UNKNOWN;
SECOidTag eku_ms_file_recovery = SEC_OID_UNKNOWN;
SECOidTag eku_ms_windows_hardware_driver_verification = SEC_OID_UNKNOWN;
SECOidTag eku_ms_qualified_subordination = SEC_OID_UNKNOWN;
SECOidTag eku_ms_key_recovery = SEC_OID_UNKNOWN;
SECOidTag eku_ms_document_signing = SEC_OID_UNKNOWN;
SECOidTag eku_ms_lifetime_signing = SEC_OID_UNKNOWN;
SECOidTag eku_ms_smart_card_logon = SEC_OID_UNKNOWN;
SECOidTag eku_ms_key_recovery_agent = SEC_OID_UNKNOWN;
SECOidTag eku_netscape_international_step_up = SEC_OID_UNKNOWN;

class DynamicOidRegisterer {
 public:
  DynamicOidRegisterer() {
    ms_cert_ext_certtype = RegisterDynamicOid("1.3.6.1.4.1.311.20.2");
    ms_certsrv_ca_version = RegisterDynamicOid("1.3.6.1.4.1.311.21.1");
    ms_nt_principal_name = RegisterDynamicOid("1.3.6.1.4.1.311.20.2.3");
    ms_ntds_replication = RegisterDynamicOid("1.3.6.1.4.1.311.25.1");

    eku_ms_individual_code_signing = RegisterDynamicOid("1.3.6.1.4.1.311.2.1.21");
    eku_ms_commercial_code_signing = RegisterDynamicOid("1.3.6.1.4.1.311.2.1.22");
    eku_ms_trust_list_signing = RegisterDynamicOid("1.3.6.1.4.1.311.10.3.1");
    eku_ms_time_stamping = RegisterDynamicOid("1.3.6.1.4.1.311.10.3.2");
    eku_ms_server_gated_crypto = RegisterDynamicOid("1.3.6.1.4.1.311.10.3.3");
    eku_ms_encrypting_file_system = RegisterDynamicOid("1.3.6.1.4.1.311.10.3.4");
    eku_ms_file_recovery = RegisterDynamicOid("1.3.6.1.4.1.311.10.3.4.1");
    eku_ms_windows_hardware_driver_verification = RegisterDynamicOid(
        "1.3.6.1.4.1.311.10.3.5");
    eku_ms_qualified_subordination = RegisterDynamicOid(
        "1.3.6.1.4.1.311.10.3.10");
    eku_ms_key_recovery = RegisterDynamicOid("1.3.6.1.4.1.311.10.3.11");
    eku_ms_document_signing = RegisterDynamicOid("1.3.6.1.4.1.311.10.3.12");
    eku_ms_lifetime_signing = RegisterDynamicOid("1.3.6.1.4.1.311.10.3.13");
    eku_ms_smart_card_logon = RegisterDynamicOid("1.3.6.1.4.1.311.20.2.2");
    eku_ms_key_recovery_agent = RegisterDynamicOid("1.3.6.1.4.1.311.21.6");
    eku_netscape_international_step_up = RegisterDynamicOid(
        "2.16.840.1.113730.4.1");
  }
};

static base::LazyInstance<DynamicOidRegisterer>::Leaky
    g_dynamic_oid_registerer = LAZY_INSTANCE_INITIALIZER;

}  // namespace

namespace mozilla_security_manager {

std::string DumpOidString(SECItem* oid) {
  char* pr_string = CERT_GetOidString(oid);
  if (pr_string) {
    std::string rv = pr_string;
    PR_smprintf_free(pr_string);
    return rv;
  }

  return ProcessRawBytes(oid);
}

std::string GetOIDText(SECItem* oid) {
  g_dynamic_oid_registerer.Get();

  int string_id;
  SECOidTag oid_tag = SECOID_FindOIDTag(oid);
  switch (oid_tag) {
    // Distinguished Name fields:
    case SEC_OID_AVA_COMMON_NAME:
      string_id = IDS_CERT_OID_AVA_COMMON_NAME;
      break;
    case SEC_OID_AVA_STATE_OR_PROVINCE:
      string_id = IDS_CERT_OID_AVA_STATE_OR_PROVINCE;
      break;
    case SEC_OID_AVA_ORGANIZATION_NAME:
      string_id = IDS_CERT_OID_AVA_ORGANIZATION_NAME;
      break;
    case SEC_OID_AVA_ORGANIZATIONAL_UNIT_NAME:
      string_id = IDS_CERT_OID_AVA_ORGANIZATIONAL_UNIT_NAME;
      break;
    case SEC_OID_AVA_DN_QUALIFIER:
      string_id = IDS_CERT_OID_AVA_DN_QUALIFIER;
      break;
    case SEC_OID_AVA_COUNTRY_NAME:
      string_id = IDS_CERT_OID_AVA_COUNTRY_NAME;
      break;
    case SEC_OID_AVA_SERIAL_NUMBER:
      string_id = IDS_CERT_OID_AVA_SERIAL_NUMBER;
      break;
    case SEC_OID_AVA_LOCALITY:
      string_id = IDS_CERT_OID_AVA_LOCALITY;
      break;
    case SEC_OID_AVA_DC:
      string_id = IDS_CERT_OID_AVA_DC;
      break;
    case SEC_OID_RFC1274_MAIL:
      string_id = IDS_CERT_OID_RFC1274_MAIL;
      break;
    case SEC_OID_RFC1274_UID:
      string_id = IDS_CERT_OID_RFC1274_UID;
      break;
    case SEC_OID_PKCS9_EMAIL_ADDRESS:
      string_id = IDS_CERT_OID_PKCS9_EMAIL_ADDRESS;
      break;

    // Extended Validation (EV) name fields:
    case SEC_OID_BUSINESS_CATEGORY:
      string_id = IDS_CERT_OID_BUSINESS_CATEGORY;
      break;
    case SEC_OID_EV_INCORPORATION_LOCALITY:
      string_id = IDS_CERT_OID_EV_INCORPORATION_LOCALITY;
      break;
    case SEC_OID_EV_INCORPORATION_STATE:
      string_id = IDS_CERT_OID_EV_INCORPORATION_STATE;
      break;
    case SEC_OID_EV_INCORPORATION_COUNTRY:
      string_id = IDS_CERT_OID_EV_INCORPORATION_COUNTRY;
      break;
    case SEC_OID_AVA_STREET_ADDRESS:
      string_id = IDS_CERT_OID_AVA_STREET_ADDRESS;
      break;
    case SEC_OID_AVA_POSTAL_CODE:
      string_id = IDS_CERT_OID_AVA_POSTAL_CODE;
      break;

    // Algorithm fields:
    case SEC_OID_PKCS1_RSA_ENCRYPTION:
      string_id = IDS_CERT_OID_PKCS1_RSA_ENCRYPTION;
      break;
    case SEC_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION:
      string_id = IDS_CERT_OID_PKCS1_MD2_WITH_RSA_ENCRYPTION;
      break;
    case SEC_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION:
      string_id = IDS_CERT_OID_PKCS1_MD4_WITH_RSA_ENCRYPTION;
      break;
    case SEC_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION:
      string_id = IDS_CERT_OID_PKCS1_MD5_WITH_RSA_ENCRYPTION;
      break;
    case SEC_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION:
      string_id = IDS_CERT_OID_PKCS1_SHA1_WITH_RSA_ENCRYPTION;
      break;
    case SEC_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION:
      string_id = IDS_CERT_OID_PKCS1_SHA256_WITH_RSA_ENCRYPTION;
      break;
    case SEC_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION:
      string_id = IDS_CERT_OID_PKCS1_SHA384_WITH_RSA_ENCRYPTION;
      break;
    case SEC_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION:
      string_id = IDS_CERT_OID_PKCS1_SHA512_WITH_RSA_ENCRYPTION;
      break;
    case SEC_OID_ANSIX962_ECDSA_SHA1_SIGNATURE:
      string_id = IDS_CERT_OID_ANSIX962_ECDSA_SHA1_SIGNATURE;
      break;
    case SEC_OID_ANSIX962_ECDSA_SHA256_SIGNATURE:
      string_id = IDS_CERT_OID_ANSIX962_ECDSA_SHA256_SIGNATURE;
      break;
    case SEC_OID_ANSIX962_ECDSA_SHA384_SIGNATURE:
      string_id = IDS_CERT_OID_ANSIX962_ECDSA_SHA384_SIGNATURE;
      break;
    case SEC_OID_ANSIX962_ECDSA_SHA512_SIGNATURE:
      string_id = IDS_CERT_OID_ANSIX962_ECDSA_SHA512_SIGNATURE;
      break;
    case SEC_OID_ANSIX962_EC_PUBLIC_KEY:
      string_id = IDS_CERT_OID_ANSIX962_EC_PUBLIC_KEY;
      break;
    case SEC_OID_SECG_EC_SECP256R1:
      string_id = IDS_CERT_OID_SECG_EC_SECP256R1;
      break;
    case SEC_OID_SECG_EC_SECP384R1:
      string_id = IDS_CERT_OID_SECG_EC_SECP384R1;
      break;
    case SEC_OID_SECG_EC_SECP521R1:
      string_id = IDS_CERT_OID_SECG_EC_SECP521R1;
      break;

    // Extension fields (including details of extensions):
    case SEC_OID_NS_CERT_EXT_CERT_TYPE:
      string_id = IDS_CERT_EXT_NS_CERT_TYPE;
      break;
    case SEC_OID_NS_CERT_EXT_BASE_URL:
      string_id = IDS_CERT_EXT_NS_CERT_BASE_URL;
      break;
    case SEC_OID_NS_CERT_EXT_REVOCATION_URL:
      string_id = IDS_CERT_EXT_NS_CERT_REVOCATION_URL;
      break;
    case SEC_OID_NS_CERT_EXT_CA_REVOCATION_URL:
      string_id = IDS_CERT_EXT_NS_CA_REVOCATION_URL;
      break;
    case SEC_OID_NS_CERT_EXT_CERT_RENEWAL_URL:
      string_id = IDS_CERT_EXT_NS_CERT_RENEWAL_URL;
      break;
    case SEC_OID_NS_CERT_EXT_CA_POLICY_URL:
      string_id = IDS_CERT_EXT_NS_CA_POLICY_URL;
      break;
    case SEC_OID_NS_CERT_EXT_SSL_SERVER_NAME:
      string_id = IDS_CERT_EXT_NS_SSL_SERVER_NAME;
      break;
    case SEC_OID_NS_CERT_EXT_COMMENT:
      string_id = IDS_CERT_EXT_NS_COMMENT;
      break;
    case SEC_OID_NS_CERT_EXT_LOST_PASSWORD_URL:
      string_id = IDS_CERT_EXT_NS_LOST_PASSWORD_URL;
      break;
    case SEC_OID_NS_CERT_EXT_CERT_RENEWAL_TIME:
      string_id = IDS_CERT_EXT_NS_CERT_RENEWAL_TIME;
      break;
    case SEC_OID_X509_SUBJECT_DIRECTORY_ATTR:
      string_id = IDS_CERT_X509_SUBJECT_DIRECTORY_ATTR;
      break;
    case SEC_OID_X509_SUBJECT_KEY_ID:
      string_id = IDS_CERT_X509_SUBJECT_KEYID;
      break;
    case SEC_OID_X509_KEY_USAGE:
      string_id = IDS_CERT_X509_KEY_USAGE;
      break;
    case SEC_OID_X509_SUBJECT_ALT_NAME:
      string_id = IDS_CERT_X509_SUBJECT_ALT_NAME;
      break;
    case SEC_OID_X509_ISSUER_ALT_NAME:
      string_id = IDS_CERT_X509_ISSUER_ALT_NAME;
      break;
    case SEC_OID_X509_BASIC_CONSTRAINTS:
      string_id = IDS_CERT_X509_BASIC_CONSTRAINTS;
      break;
    case SEC_OID_X509_NAME_CONSTRAINTS:
      string_id = IDS_CERT_X509_NAME_CONSTRAINTS;
      break;
    case SEC_OID_X509_CRL_DIST_POINTS:
      string_id = IDS_CERT_X509_CRL_DIST_POINTS;
      break;
    case SEC_OID_X509_CERTIFICATE_POLICIES:
      string_id = IDS_CERT_X509_CERT_POLICIES;
      break;
    case SEC_OID_X509_POLICY_MAPPINGS:
      string_id = IDS_CERT_X509_POLICY_MAPPINGS;
      break;
    case SEC_OID_X509_POLICY_CONSTRAINTS:
      string_id = IDS_CERT_X509_POLICY_CONSTRAINTS;
      break;
    case SEC_OID_X509_AUTH_KEY_ID:
      string_id = IDS_CERT_X509_AUTH_KEYID;
      break;
    case SEC_OID_X509_EXT_KEY_USAGE:
      string_id = IDS_CERT_X509_EXT_KEY_USAGE;
      break;
    case SEC_OID_X509_AUTH_INFO_ACCESS:
      string_id = IDS_CERT_X509_AUTH_INFO_ACCESS;
      break;
    case SEC_OID_PKIX_CPS_POINTER_QUALIFIER:
      string_id = IDS_CERT_PKIX_CPS_POINTER_QUALIFIER;
      break;
    case SEC_OID_PKIX_USER_NOTICE_QUALIFIER:
      string_id = IDS_CERT_PKIX_USER_NOTICE_QUALIFIER;
      break;

    // Extended Key Usages:
    case SEC_OID_EXT_KEY_USAGE_SERVER_AUTH:
      string_id = IDS_CERT_EKU_TLS_WEB_SERVER_AUTHENTICATION;
      break;
    case SEC_OID_EXT_KEY_USAGE_CLIENT_AUTH:
      string_id = IDS_CERT_EKU_TLS_WEB_CLIENT_AUTHENTICATION;
      break;
    case SEC_OID_EXT_KEY_USAGE_CODE_SIGN:
      string_id = IDS_CERT_EKU_CODE_SIGNING;
      break;
    case SEC_OID_EXT_KEY_USAGE_EMAIL_PROTECT:
      string_id = IDS_CERT_EKU_EMAIL_PROTECTION;
      break;
    case SEC_OID_EXT_KEY_USAGE_TIME_STAMP:
      string_id = IDS_CERT_EKU_TIME_STAMPING;
      break;
    case SEC_OID_OCSP_RESPONDER:
      string_id = IDS_CERT_EKU_OCSP_SIGNING;
      break;

    // Explicitly handle UNKNOWN to avoid the conditional below.
    case SEC_OID_UNKNOWN:
      string_id = -1;
      break;

    // OIDs that are not directly registered with NSS, and thus cannot be
    // used as part of a switch tag. While there is a potentially boundless
    // set here, only list ones that either other platforms list or which
    // might otherwise be encountered in the Web PKI or mainstream Enterprise
    // deployments.
    default:
      if (oid_tag == ms_cert_ext_certtype)
        string_id = IDS_CERT_EXT_MS_CERT_TYPE;
      else if (oid_tag == ms_certsrv_ca_version)
        string_id = IDS_CERT_EXT_MS_CA_VERSION;
      else if (oid_tag == ms_nt_principal_name)
        string_id = IDS_CERT_EXT_MS_NT_PRINCIPAL_NAME;
      else if (oid_tag == ms_ntds_replication)
        string_id = IDS_CERT_EXT_MS_NTDS_REPLICATION;
      else if (oid_tag == eku_ms_individual_code_signing)
        string_id = IDS_CERT_EKU_MS_INDIVIDUAL_CODE_SIGNING;
      else if (oid_tag == eku_ms_commercial_code_signing)
        string_id = IDS_CERT_EKU_MS_COMMERCIAL_CODE_SIGNING;
      else if (oid_tag == eku_ms_trust_list_signing)
        string_id = IDS_CERT_EKU_MS_TRUST_LIST_SIGNING;
      else if (oid_tag == eku_ms_time_stamping)
        string_id = IDS_CERT_EKU_MS_TIME_STAMPING;
      else if (oid_tag == eku_ms_server_gated_crypto)
        string_id = IDS_CERT_EKU_MS_SERVER_GATED_CRYPTO;
      else if (oid_tag == eku_ms_encrypting_file_system)
        string_id = IDS_CERT_EKU_MS_ENCRYPTING_FILE_SYSTEM;
      else if (oid_tag == eku_ms_file_recovery)
        string_id = IDS_CERT_EKU_MS_FILE_RECOVERY;
      else if (oid_tag == eku_ms_windows_hardware_driver_verification)
        string_id = IDS_CERT_EKU_MS_WINDOWS_HARDWARE_DRIVER_VERIFICATION;
      else if (oid_tag == eku_ms_qualified_subordination)
        string_id = IDS_CERT_EKU_MS_QUALIFIED_SUBORDINATION;
      else if (oid_tag == eku_ms_key_recovery)
        string_id = IDS_CERT_EKU_MS_KEY_RECOVERY;
      else if (oid_tag == eku_ms_document_signing)
        string_id = IDS_CERT_EKU_MS_DOCUMENT_SIGNING;
      else if (oid_tag == eku_ms_lifetime_signing)
        string_id = IDS_CERT_EKU_MS_LIFETIME_SIGNING;
      else if (oid_tag == eku_ms_smart_card_logon)
        string_id = IDS_CERT_EKU_MS_SMART_CARD_LOGON;
      else if (oid_tag == eku_ms_key_recovery_agent)
        string_id = IDS_CERT_EKU_MS_KEY_RECOVERY_AGENT;
      else if (oid_tag == eku_netscape_international_step_up)
        string_id = IDS_CERT_EKU_NETSCAPE_INTERNATIONAL_STEP_UP;
      else
        string_id = -1;
      break;
  }
  if (string_id >= 0)
    return l10n_util::GetStringUTF8(string_id);

  return DumpOidString(oid);
}

// Get a display string from a Relative Distinguished Name.
std::string ProcessRDN(CERTRDN* rdn) {
  std::string rv;

  CERTAVA** avas = rdn->avas;
  for (size_t i = 0; avas[i] != NULL; ++i) {
    rv += GetOIDText(&avas[i]->type);
    SECItem* decode_item = CERT_DecodeAVAValue(&avas[i]->value);
    if (decode_item) {
      // TODO(mattm): Pass decode_item to CERT_RFC1485_EscapeAndQuote.
      rv += " = ";
      std::string value(reinterpret_cast<char*>(decode_item->data),
                        decode_item->len);
      if (SECOID_FindOIDTag(&avas[i]->type) == SEC_OID_AVA_COMMON_NAME)
        value = x509_certificate_model::ProcessIDN(value);
      rv += value;
      SECITEM_FreeItem(decode_item, PR_TRUE);
    }
    rv += '\n';
  }

  return rv;
}

std::string ProcessName(CERTName* name) {
  std::string rv;
  CERTRDN** last_rdn;

  // Find last non-NULL rdn.
  for (last_rdn = name->rdns; last_rdn[0]; last_rdn++) {}
  last_rdn--;

  for (CERTRDN** rdn = last_rdn; rdn >= name->rdns; rdn--)
    rv += ProcessRDN(*rdn);
  return rv;
}

std::string ProcessBasicConstraints(SECItem* extension_data) {
  CERTBasicConstraints value;
  value.pathLenConstraint = -1;
  if (CERT_DecodeBasicConstraintValue(&value, extension_data) != SECSuccess)
    return ProcessRawBytes(extension_data);

  std::string rv;
  if (value.isCA)
    rv = l10n_util::GetStringUTF8(IDS_CERT_X509_BASIC_CONSTRAINT_IS_CA);
  else
    rv = l10n_util::GetStringUTF8(IDS_CERT_X509_BASIC_CONSTRAINT_IS_NOT_CA);
  rv += '\n';
  if (value.pathLenConstraint != -1) {
    std::u16string depth;
    if (value.pathLenConstraint == CERT_UNLIMITED_PATH_CONSTRAINT) {
      depth = l10n_util::GetStringUTF16(
          IDS_CERT_X509_BASIC_CONSTRAINT_PATH_LEN_UNLIMITED);
    } else {
      depth = base::FormatNumber(value.pathLenConstraint);
    }
    rv += l10n_util::GetStringFUTF8(IDS_CERT_X509_BASIC_CONSTRAINT_PATH_LEN,
                                    depth);
  }
  return rv;
}

std::string ProcessGeneralName(PRArenaPool* arena,
                               CERTGeneralName* current) {
  DCHECK(current);

  std::string key;
  std::string value;

  switch (current->type) {
    case certOtherName: {
      key = GetOIDText(&current->name.OthName.oid);
      // g_dynamic_oid_registerer.Get() will have been run by GetOIDText.
      SECOidTag oid_tag = SECOID_FindOIDTag(&current->name.OthName.oid);
      if (oid_tag == ms_nt_principal_name) {
        // The type of this name is apparently nowhere explicitly
        // documented. However, in the generated templates, it is always
        // UTF-8. So try to decode this as UTF-8; if that fails, dump the
        // raw data.
        SECItem decoded;
        if (SEC_ASN1DecodeItem(arena, &decoded,
                               SEC_ASN1_GET(SEC_UTF8StringTemplate),
                               &current->name.OthName.name) == SECSuccess) {
          value = std::string(reinterpret_cast<char*>(decoded.data),
                              decoded.len);
        } else {
          value = ProcessRawBytes(&current->name.OthName.name);
        }
        break;
      } else if (oid_tag == ms_ntds_replication) {
        // This should be a 16-byte GUID.
        SECItem guid;
        if (SEC_ASN1DecodeItem(arena, &guid,
                               SEC_ASN1_GET(SEC_OctetStringTemplate),
                               &current->name.OthName.name) == SECSuccess &&
            guid.len == 16) {
          unsigned char* d = guid.data;
          base::SStringPrintf(
              &value,
              "{%.2x%.2x%.2x%.2x-%.2x%.2x-%.2x%.2x-"
              "%.2x%.2x-%.2x%.2x%.2x%.2x%.2x%.2x}",
              d[3], d[2], d[1], d[0], d[5], d[4], d[7], d[6],
              d[8], d[9], d[10], d[11], d[12], d[13], d[14], d[15]);
        } else {
          value = ProcessRawBytes(&current->name.OthName.name);
        }
      } else {
        value = ProcessRawBytes(&current->name.OthName.name);
      }
      break;
    }
    case certRFC822Name:
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_RFC822_NAME);
      value = std::string(reinterpret_cast<char*>(current->name.other.data),
                          current->name.other.len);
      break;
    case certDNSName:
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_DNS_NAME);
      value = std::string(reinterpret_cast<char*>(current->name.other.data),
                          current->name.other.len);
      value = x509_certificate_model::ProcessIDN(value);
      break;
    case certX400Address:
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_X400_ADDRESS);
      value = ProcessRawBytes(&current->name.other);
      break;
    case certDirectoryName:
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_DIRECTORY_NAME);
      value = ProcessName(&current->name.directoryName);
      break;
    case certEDIPartyName:
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_EDI_PARTY_NAME);
      value = ProcessRawBytes(&current->name.other);
      break;
    case certURI:
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_URI);
      value = std::string(reinterpret_cast<char*>(current->name.other.data),
                          current->name.other.len);
      break;
    case certIPAddress: {
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_IP_ADDRESS);

      net::IPAddress ip(current->name.other.data, current->name.other.len);
      if (ip.IsValid()) {
        value = ip.ToString();
      } else {
        // Invalid IP address.
        value = ProcessRawBytes(&current->name.other);
      }
      break;
    }
    case certRegisterID:
      key = l10n_util::GetStringUTF8(IDS_CERT_GENERAL_NAME_REGISTERED_ID);
      value = DumpOidString(&current->name.other);
      break;
  }
  std::string rv(l10n_util::GetStringFUTF8(IDS_CERT_UNKNOWN_OID_INFO_FORMAT,
                                           base::UTF8ToUTF16(key),
                                           base::UTF8ToUTF16(value)));
  rv += '\n';
  return rv;
}

std::string ProcessGeneralNames(PRArenaPool* arena,
                                CERTGeneralName* name_list) {
  std::string rv;
  CERTGeneralName* current = name_list;

  do {
    std::string text = ProcessGeneralName(arena, current);
    if (text.empty())
      break;
    rv += text;
    current = CERT_GetNextGeneralName(current);
  } while (current != name_list);
  return rv;
}

std::string ProcessAltName(SECItem* extension_data) {
  CERTGeneralName* name_list;

  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  CHECK(arena.get());

  name_list = CERT_DecodeAltNameExtension(arena.get(), extension_data);
  if (!name_list)
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);

  return ProcessGeneralNames(arena.get(), name_list);
}

std::string ProcessSubjectKeyId(SECItem* extension_data) {
  SECItem decoded;
  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  CHECK(arena.get());

  std::string rv;
  if (SEC_QuickDERDecodeItem(arena.get(), &decoded,
                             SEC_ASN1_GET(SEC_OctetStringTemplate),
                             extension_data) != SECSuccess) {
    rv = l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);
    return rv;
  }

  rv = l10n_util::GetStringFUTF8(IDS_CERT_KEYID_FORMAT,
                                 base::ASCIIToUTF16(ProcessRawBytes(&decoded)));
  return rv;
}

std::string ProcessAuthKeyId(SECItem* extension_data) {
  CERTAuthKeyID* ret;
  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  std::string rv;

  CHECK(arena.get());

  ret = CERT_DecodeAuthKeyID(arena.get(), extension_data);

  if (ret->keyID.len > 0) {
    rv += l10n_util::GetStringFUTF8(IDS_CERT_KEYID_FORMAT,
                                    base::ASCIIToUTF16(
                                        ProcessRawBytes(&ret->keyID)));
    rv += '\n';
  }

  if (ret->authCertIssuer) {
    rv += l10n_util::GetStringFUTF8(
        IDS_CERT_ISSUER_FORMAT,
        base::UTF8ToUTF16(
            ProcessGeneralNames(arena.get(), ret->authCertIssuer)));
    rv += '\n';
  }

  if (ret->authCertSerialNumber.len > 0) {
    rv += l10n_util::GetStringFUTF8(
        IDS_CERT_SERIAL_NUMBER_FORMAT,
        base::ASCIIToUTF16(ProcessRawBytes(&ret->authCertSerialNumber)));
    rv += '\n';
  }

  return rv;
}

std::string ProcessUserNotice(SECItem* der_notice) {
  CERTUserNotice* notice = CERT_DecodeUserNotice(der_notice);
  if (!notice)
    return ProcessRawBytes(der_notice);

  std::string rv;
  if (notice->noticeReference.organization.len != 0) {
    switch (notice->noticeReference.organization.type) {
      case siAsciiString:
      case siVisibleString:
      case siUTF8String:
        rv += std::string(
            reinterpret_cast<char*>(notice->noticeReference.organization.data),
            notice->noticeReference.organization.len);
        break;
      case siBMPString:
        rv += ProcessBMPString(&notice->noticeReference.organization);
        break;
      default:
        break;
    }
    rv += " - ";
    SECItem** itemList = notice->noticeReference.noticeNumbers;
    while (*itemList) {
      unsigned long number;
      if (SEC_ASN1DecodeInteger(*itemList, &number) == SECSuccess) {
        if (itemList != notice->noticeReference.noticeNumbers)
          rv += ", ";
        rv += '#';
        rv += base::NumberToString(number);
      }
      itemList++;
    }
  }
  if (notice->displayText.len != 0) {
    rv += "\n    ";
    switch (notice->displayText.type) {
      case siAsciiString:
      case siVisibleString:
      case siUTF8String:
        rv += std::string(reinterpret_cast<char*>(notice->displayText.data),
                          notice->displayText.len);
        break;
      case siBMPString:
        rv += ProcessBMPString(&notice->displayText);
        break;
      default:
        break;
    }
  }

  CERT_DestroyUserNotice(notice);
  return rv;
}

std::string ProcessCertificatePolicies(SECItem* extension_data) {
  std::string rv;

  CERTCertificatePolicies* policies = CERT_DecodeCertificatePoliciesExtension(
      extension_data);
  if (!policies)
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);

  CERTPolicyInfo** policyInfos = policies->policyInfos;
  while (*policyInfos) {
    CERTPolicyInfo* policyInfo = *policyInfos++;
    std::string key = GetOIDText(&policyInfo->policyID);

    // If we have policy qualifiers, display the oid text
    // with a ':', otherwise just put the oid text and a newline.
    // TODO(mattm): Add extra note if this is the ev oid?  (It's a bit
    // complicated, since we don't want to do the EV check synchronously.)
    if (policyInfo->policyQualifiers) {
      rv += l10n_util::GetStringFUTF8(IDS_CERT_MULTILINE_INFO_START_FORMAT,
                                      base::UTF8ToUTF16(key));
    } else {
      rv += key;
    }
    rv += '\n';

    if (policyInfo->policyQualifiers) {
      // Add all qualifiers on separate lines, indented.
      CERTPolicyQualifier** policyQualifiers = policyInfo->policyQualifiers;
      while (*policyQualifiers != NULL) {
        rv += "  ";

        CERTPolicyQualifier* policyQualifier = *policyQualifiers++;
        rv += l10n_util::GetStringFUTF8(
            IDS_CERT_MULTILINE_INFO_START_FORMAT,
            base::UTF8ToUTF16(GetOIDText(&policyQualifier->qualifierID)));
        switch(policyQualifier->oid) {
          case SEC_OID_PKIX_CPS_POINTER_QUALIFIER:
            rv += "    ";
            /* The CPS pointer ought to be the cPSuri alternative
               of the Qualifier choice. */
            rv += ProcessIA5String(&policyQualifier->qualifierValue);
            break;
          case SEC_OID_PKIX_USER_NOTICE_QUALIFIER:
            rv += ProcessUserNotice(&policyQualifier->qualifierValue);
            break;
          default:
            rv += ProcessRawBytes(&policyQualifier->qualifierValue);
            break;
        }
        rv += '\n';
      }
    }
  }

  CERT_DestroyCertificatePoliciesExtension(policies);
  return rv;
}

std::string ProcessCrlDistPoints(SECItem* extension_data) {
  std::string rv;
  CERTCrlDistributionPoints* crldp;
  CRLDistributionPoint** points;
  CRLDistributionPoint* point;
  bool comma;

  static const struct {
    int reason;
    int string_id;
  } reason_string_map[] = {
    {RF_UNUSED, IDS_CERT_REVOCATION_REASON_UNUSED},
    {RF_KEY_COMPROMISE, IDS_CERT_REVOCATION_REASON_KEY_COMPROMISE},
    {RF_CA_COMPROMISE, IDS_CERT_REVOCATION_REASON_CA_COMPROMISE},
    {RF_AFFILIATION_CHANGED, IDS_CERT_REVOCATION_REASON_AFFILIATION_CHANGED},
    {RF_SUPERSEDED, IDS_CERT_REVOCATION_REASON_SUPERSEDED},
    {RF_CESSATION_OF_OPERATION,
     IDS_CERT_REVOCATION_REASON_CESSATION_OF_OPERATION},
    {RF_CERTIFICATE_HOLD, IDS_CERT_REVOCATION_REASON_CERTIFICATE_HOLD},
  };

  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  CHECK(arena.get());

  crldp = CERT_DecodeCRLDistributionPoints(arena.get(), extension_data);
  if (!crldp || !crldp->distPoints) {
    rv = l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);
    return rv;
  }

  for (points = crldp->distPoints; *points; ++points) {
    point = *points;
    switch (point->distPointType) {
      case generalName:
        // generalName is a typo in upstream NSS; fullName is actually a
        // GeneralNames (SEQUENCE OF GeneralName). See Mozilla Bug #615100.
        rv += ProcessGeneralNames(arena.get(), point->distPoint.fullName);
        break;
      case relativeDistinguishedName:
        rv += ProcessRDN(&point->distPoint.relativeName);
        break;
    }
    if (point->reasons.len) {
      rv += ' ';
      comma = false;
      for (size_t i = 0; i < base::size(reason_string_map); ++i) {
        if (point->reasons.data[0] & reason_string_map[i].reason) {
          if (comma)
            rv += ',';
          rv += l10n_util::GetStringUTF8(reason_string_map[i].string_id);
          comma = true;
        }
      }
      rv += '\n';
    }
    if (point->crlIssuer) {
      rv += l10n_util::GetStringFUTF8(
          IDS_CERT_ISSUER_FORMAT,
          base::UTF8ToUTF16(
              ProcessGeneralNames(arena.get(), point->crlIssuer)));
    }
  }
  return rv;
}

std::string ProcessAuthInfoAccess(SECItem* extension_data) {
  std::string rv;
  CERTAuthInfoAccess** aia;
  CERTAuthInfoAccess* desc;
  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  CHECK(arena.get());

  aia = CERT_DecodeAuthInfoAccessExtension(arena.get(), extension_data);
  if (aia == NULL)
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);

  while (*aia != NULL) {
    desc = *aia++;
    std::u16string location_str =
        base::UTF8ToUTF16(ProcessGeneralName(arena.get(), desc->location));
    switch (SECOID_FindOIDTag(&desc->method)) {
    case SEC_OID_PKIX_OCSP:
      rv += l10n_util::GetStringFUTF8(IDS_CERT_OCSP_RESPONDER_FORMAT,
                                      location_str);
      break;
    case SEC_OID_PKIX_CA_ISSUERS:
      rv += l10n_util::GetStringFUTF8(IDS_CERT_CA_ISSUERS_FORMAT,
                                      location_str);
      break;
    default:
      rv += l10n_util::GetStringFUTF8(IDS_CERT_UNKNOWN_OID_INFO_FORMAT,
                                      base::UTF8ToUTF16(
                                          GetOIDText(&desc->method)),
                                      location_str);
      break;
    }
  }
  return rv;
}

std::string ProcessIA5String(SECItem* extension_data) {
  SECItem item;
  if (SEC_ASN1DecodeItem(NULL, &item, SEC_ASN1_GET(SEC_IA5StringTemplate),
                         extension_data) != SECSuccess)
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);
  std::string rv((char*)item.data, item.len);  // ASCII data.
  PORT_Free(item.data);
  return rv;
}

std::string ProcessBMPString(SECItem* extension_data) {
  std::string rv;
  SECItem item;
  crypto::ScopedPLArenaPool arena(PORT_NewArena(DER_DEFAULT_CHUNKSIZE));
  CHECK(arena.get());

  if (SEC_ASN1DecodeItem(arena.get(), &item,
                         SEC_ASN1_GET(SEC_BMPStringTemplate), extension_data) ==
      SECSuccess)
    rv = BMPtoUTF8(arena.get(), item.data, item.len);
  return rv;
}

struct MaskIdPair {
  unsigned int mask;
  int string_id;
};

static std::string ProcessBitField(SECItem* bitfield,
                                   const MaskIdPair* string_map,
                                   size_t len,
                                   char separator) {
  unsigned int bits = 0;
  std::string rv;
  for (size_t i = 0; i * 8 < bitfield->len && i < sizeof(bits); ++i)
    bits |= bitfield->data[i] << (i * 8);
  for (size_t i = 0; i < len; ++i) {
    if (bits & string_map[i].mask) {
      if (!rv.empty())
        rv += separator;
      rv += l10n_util::GetStringUTF8(string_map[i].string_id);
    }
  }
  return rv;
}

static std::string ProcessBitStringExtension(SECItem* extension_data,
                                             const MaskIdPair* string_map,
                                             size_t len,
                                             char separator) {
  SECItem decoded;
  decoded.type = siBuffer;
  decoded.data = NULL;
  decoded.len  = 0;
  if (SEC_ASN1DecodeItem(NULL, &decoded, SEC_ASN1_GET(SEC_BitStringTemplate),
                         extension_data) != SECSuccess)
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);
  std::string rv = ProcessBitField(&decoded, string_map, len, separator);
  PORT_Free(decoded.data);
  return rv;
}

std::string ProcessNSCertTypeExtension(SECItem* extension_data) {
  static const MaskIdPair usage_string_map[] = {
    {NS_CERT_TYPE_SSL_CLIENT, IDS_CERT_USAGE_SSL_CLIENT},
    {NS_CERT_TYPE_SSL_SERVER, IDS_CERT_USAGE_SSL_SERVER},
    {NS_CERT_TYPE_EMAIL, IDS_CERT_EXT_NS_CERT_TYPE_EMAIL},
    {NS_CERT_TYPE_OBJECT_SIGNING, IDS_CERT_USAGE_OBJECT_SIGNER},
    {NS_CERT_TYPE_SSL_CA, IDS_CERT_USAGE_SSL_CA},
    {NS_CERT_TYPE_EMAIL_CA, IDS_CERT_EXT_NS_CERT_TYPE_EMAIL_CA},
    {NS_CERT_TYPE_OBJECT_SIGNING_CA, IDS_CERT_USAGE_OBJECT_SIGNER},
  };
  return ProcessBitStringExtension(extension_data, usage_string_map,
                                   base::size(usage_string_map), '\n');
}

static const MaskIdPair key_usage_string_map[] = {
  {KU_DIGITAL_SIGNATURE, IDS_CERT_X509_KEY_USAGE_SIGNING},
  {KU_NON_REPUDIATION, IDS_CERT_X509_KEY_USAGE_NONREP},
  {KU_KEY_ENCIPHERMENT, IDS_CERT_X509_KEY_USAGE_ENCIPHERMENT},
  {KU_DATA_ENCIPHERMENT, IDS_CERT_X509_KEY_USAGE_DATA_ENCIPHERMENT},
  {KU_KEY_AGREEMENT, IDS_CERT_X509_KEY_USAGE_KEY_AGREEMENT},
  {KU_KEY_CERT_SIGN, IDS_CERT_X509_KEY_USAGE_CERT_SIGNER},
  {KU_CRL_SIGN, IDS_CERT_X509_KEY_USAGE_CRL_SIGNER},
  {KU_ENCIPHER_ONLY, IDS_CERT_X509_KEY_USAGE_ENCIPHER_ONLY},
  // NSS is missing a flag for dechiperOnly, see:
  // https://bugzilla.mozilla.org/show_bug.cgi?id=549952
};

std::string ProcessKeyUsageBitString(SECItem* bitstring, char sep) {
  return ProcessBitField(bitstring, key_usage_string_map,
                         base::size(key_usage_string_map), sep);
}

std::string ProcessKeyUsageExtension(SECItem* extension_data) {
  return ProcessBitStringExtension(extension_data, key_usage_string_map,
                                   base::size(key_usage_string_map), '\n');
}

std::string ProcessExtKeyUsage(SECItem* extension_data) {
  std::string rv;
  CERTOidSequence* extension_key_usage = NULL;
  extension_key_usage = CERT_DecodeOidSequence(extension_data);
  if (extension_key_usage == NULL)
    return l10n_util::GetStringUTF8(IDS_CERT_EXTENSION_DUMP_ERROR);

  SECItem** oids;
  SECItem* oid;
  for (oids = extension_key_usage->oids; oids != NULL && *oids != NULL;
       ++oids) {
    oid = *oids;
    std::string oid_dump = DumpOidString(oid);
    std::string oid_text = GetOIDText(oid);

    // If oid is one we recognize, oid_text will have a text description of the
    // OID, which we display along with the oid_dump.  If we don't recognize the
    // OID, GetOIDText will return the same value as DumpOidString, so just
    // display the OID alone.
    if (oid_dump == oid_text)
      rv += oid_dump;
    else
      rv += l10n_util::GetStringFUTF8(IDS_CERT_EXT_KEY_USAGE_FORMAT,
                                      base::UTF8ToUTF16(oid_text),
                                      base::UTF8ToUTF16(oid_dump));
    rv += '\n';
  }
  CERT_DestroyOidSequence(extension_key_usage);
  return rv;
}

std::string ProcessExtensionData(CERTCertExtension* extension) {
  g_dynamic_oid_registerer.Get();
  SECOidTag oid_tag = SECOID_FindOIDTag(&extension->id);
  SECItem* extension_data = &extension->value;

  // This (and its sub-functions) are based on the same-named functions in
  // security/manager/ssl/src/nsNSSCertHelper.cpp.
  switch (oid_tag) {
    case SEC_OID_NS_CERT_EXT_CERT_TYPE:
      return ProcessNSCertTypeExtension(extension_data);
    case SEC_OID_X509_KEY_USAGE:
      return ProcessKeyUsageExtension(extension_data);
    case SEC_OID_X509_BASIC_CONSTRAINTS:
      return ProcessBasicConstraints(extension_data);
    case SEC_OID_X509_EXT_KEY_USAGE:
      return ProcessExtKeyUsage(extension_data);
    case SEC_OID_X509_ISSUER_ALT_NAME:
    case SEC_OID_X509_SUBJECT_ALT_NAME:
      return ProcessAltName(extension_data);
    case SEC_OID_X509_SUBJECT_KEY_ID:
      return ProcessSubjectKeyId(extension_data);
    case SEC_OID_X509_AUTH_KEY_ID:
      return ProcessAuthKeyId(extension_data);
    case SEC_OID_X509_CERTIFICATE_POLICIES:
      return ProcessCertificatePolicies(extension_data);
    case SEC_OID_X509_CRL_DIST_POINTS:
      return ProcessCrlDistPoints(extension_data);
    case SEC_OID_X509_AUTH_INFO_ACCESS:
      return ProcessAuthInfoAccess(extension_data);
    case SEC_OID_NS_CERT_EXT_BASE_URL:
    case SEC_OID_NS_CERT_EXT_REVOCATION_URL:
    case SEC_OID_NS_CERT_EXT_CA_REVOCATION_URL:
    case SEC_OID_NS_CERT_EXT_CA_CERT_URL:
    case SEC_OID_NS_CERT_EXT_CERT_RENEWAL_URL:
    case SEC_OID_NS_CERT_EXT_CA_POLICY_URL:
    case SEC_OID_NS_CERT_EXT_HOMEPAGE_URL:
    case SEC_OID_NS_CERT_EXT_COMMENT:
    case SEC_OID_NS_CERT_EXT_SSL_SERVER_NAME:
    case SEC_OID_NS_CERT_EXT_LOST_PASSWORD_URL:
      return ProcessIA5String(extension_data);
    default:
      if (oid_tag == ms_cert_ext_certtype)
        return ProcessBMPString(extension_data);
      return ProcessRawBytes(extension_data);
  }
}

std::string ProcessSubjectPublicKeyInfo(CERTSubjectPublicKeyInfo* spki) {
  std::string rv;
  SECKEYPublicKey* key = SECKEY_ExtractPublicKey(spki);
  if (key) {
    switch (key->keyType) {
      case rsaKey: {
        rv = l10n_util::GetStringFUTF8(
            IDS_CERT_RSA_PUBLIC_KEY_DUMP_FORMAT,
            base::NumberToString16(key->u.rsa.modulus.len * 8),
            base::UTF8ToUTF16(ProcessRawBytes(&key->u.rsa.modulus)),
            base::NumberToString16(key->u.rsa.publicExponent.len * 8),
            base::UTF8ToUTF16(ProcessRawBytes(&key->u.rsa.publicExponent)));
        break;
      }
      default:
        rv = x509_certificate_model::ProcessRawBits(
            spki->subjectPublicKey.data, spki->subjectPublicKey.len);
        break;
    }
    SECKEY_DestroyPublicKey(key);
  }
  return rv;
}

net::CertType GetCertType(CERTCertificate *cert) {
  CERTCertTrust trust = {0};
  CERT_GetCertTrust(cert, &trust);

  unsigned all_flags = trust.sslFlags | trust.emailFlags |
      trust.objectSigningFlags;

  if (cert->nickname && (all_flags & CERTDB_USER))
    return net::USER_CERT;
  if ((all_flags & CERTDB_VALID_CA) || CERT_IsCACert(cert, NULL))
    return net::CA_CERT;
  // TODO(mattm): http://crbug.com/128633.
  if (trust.sslFlags & CERTDB_TERMINAL_RECORD)
    return net::SERVER_CERT;
  return net::OTHER_CERT;
}

}  // namespace mozilla_security_manager
