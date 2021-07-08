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
 *  John Gardiner Myers <jgmyers@speakeasy.net>
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

#include "chrome/third_party/mozilla_security_manager/nsUsageArrayHelper.h"

#include <stddef.h>

#include "base/cxx17_backports.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace mozilla_security_manager {

void GetCertUsageStrings(CERTCertificate* cert, std::vector<std::string>* out) {
  SECCertificateUsage usages = 0;
  // TODO(wtc): See if we should use X509Certificate::Verify instead.
  if (CERT_VerifyCertificateNow(CERT_GetDefaultCertDB(), cert, PR_TRUE,
                                certificateUsageCheckAllUsages,
                                NULL, &usages) == SECSuccess) {
    static const struct {
      SECCertificateUsage usage;
      int string_id;
    } usage_string_map[] = {
      {certificateUsageSSLClient, IDS_CERT_USAGE_SSL_CLIENT},
      {certificateUsageSSLServer, IDS_CERT_USAGE_SSL_SERVER},
      {certificateUsageSSLServerWithStepUp,
        IDS_CERT_USAGE_SSL_SERVER_WITH_STEPUP},
      {certificateUsageEmailSigner, IDS_CERT_USAGE_EMAIL_SIGNER},
      {certificateUsageEmailRecipient, IDS_CERT_USAGE_EMAIL_RECEIVER},
      {certificateUsageObjectSigner, IDS_CERT_USAGE_OBJECT_SIGNER},
      {certificateUsageSSLCA, IDS_CERT_USAGE_SSL_CA},
      {certificateUsageStatusResponder, IDS_CERT_USAGE_STATUS_RESPONDER},
    };
    for (size_t i = 0; i < base::size(usage_string_map); ++i) {
      if (usages & usage_string_map[i].usage)
        out->push_back(l10n_util::GetStringUTF8(
            usage_string_map[i].string_id));
    }
  }
}

}  // namespace mozilla_security_manager
