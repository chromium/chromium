// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "components/signin/public/identity_manager/account_managed_status_finder.h"

#include "base/containers/fixed_flat_set.h"
#include "base/logging.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace signin {

namespace {

const char* g_non_managed_domain_for_testing = nullptr;

std::string GetDomainFromEmail(const std::string& email) {
  size_t email_separator_pos = email.find('@');
  if (email.empty() || email_separator_pos == std::string::npos ||
      email_separator_pos == email.size() - 1) {
    // An empty email means no logged-in user, or incognito user in case of
    // ChromiumOS. Also, some tests use nonsense email addresses (e.g. "test").
    return std::string();
  }
  return gaia::ExtractDomainName(email);
}

bool IsGmailDomain(const std::string& email_domain) {
  static constexpr auto kGmailDomains =
      base::MakeFixedFlatSet<std::string_view>({"gmail.com", "googlemail.com"});

  return kGmailDomains.contains(email_domain);
}

bool IsGmailUserBasedOnEmail(const std::string& email) {
  return IsGmailDomain(GetDomainFromEmail(email));
}

}  // namespace

// static
bool AccountManagedStatusFinder::MayBeEnterpriseDomain(
    const std::string& email_domain) {
  // List of consumer-only domains from the server side logic. See
  // `KNOWN_INVALID_DOMAINS` from GetAgencySignupStateProducerModule.java.
  // Sharing this in open source Chromium code was green-lighted according to
  // https://chromium-review.googlesource.com/c/chromium/src/+/2945029/comment/d8731200_4064534e/
  static constexpr auto kKnownConsumerDomains =
      base::MakeFixedFlatSet<std::string_view>({"123mail.org",
                                                "150mail.com",
                                                "150ml.com",
                                                "16mail.com",
                                                "2-mail.com",
                                                "2trom.com",
                                                "4email.net",
                                                "50mail.com",
                                                "aapt.net.au",
                                                "accountant.com",
                                                "acdcfan.com",
                                                "activist.com",
                                                "adam.com.au",
                                                "adexec.com",
                                                "africamail.com",
                                                "aircraftmail.com",
                                                "airpost.net",
                                                "allergist.com",
                                                "allmail.net",
                                                "alumni.com",
                                                "alumnidirector.com",
                                                "angelic.com",
                                                "anonymous.to",
                                                "aol.com",
                                                "appraiser.net",
                                                "archaeologist.com",
                                                "arcticmail.com",
                                                "artlover.com",
                                                "asia-mail.com",
                                                "asia.com",
                                                "atheist.com",
                                                "auctioneer.net",
                                                "australiamail.com",
                                                "bartender.net",
                                                "bellair.net",
                                                "berlin.com",
                                                "bestmail.us",
                                                "bigpond.com",
                                                "bigpond.com.au",
                                                "bigpond.net.au",
                                                "bikerider.com",
                                                "birdlover.com",
                                                "blader.com",
                                                "boardermail.com",
                                                "brazilmail.com",
                                                "brew-master.com",  // nocheck
                                                "brew-meister.com",
                                                "bsdmail.com",
                                                "californiamail.com",
                                                "cash4u.com",
                                                "catlover.com",
                                                "cheerful.com",
                                                "chef.net",
                                                "chemist.com",
                                                "chinamail.com",
                                                "clerk.com",
                                                "clubmember.org",
                                                "cluemail.com",
                                                "collector.org",
                                                "columnist.com",
                                                "comcast.net",
                                                "comic.com",
                                                "computer4u.com",
                                                "consultant.com",
                                                "contractor.net",
                                                "coolsite.net",
                                                "counsellor.com",
                                                "cutey.com",
                                                "cyber-wizard.com",
                                                "cyberdude.com",
                                                "cybergal.com",
                                                "cyberservices.com",
                                                "dallasmail.com",
                                                "dbzmail.com",
                                                "deliveryman.com",
                                                "diplomats.com",
                                                "disciples.com",
                                                "discofan.com",
                                                "disposable.com",
                                                "dispostable.com",
                                                "dodo.com.au",
                                                "doglover.com",
                                                "doramail.com",
                                                "dr.com",
                                                "dublin.com",
                                                "dutchmail.com",
                                                "earthlink.net",
                                                "elitemail.org",
                                                "elvisfan.com",
                                                "email.com",
                                                "emailcorner.net",
                                                "emailengine.net",
                                                "emailengine.org",
                                                "emailgroups.net",
                                                "emailplus.org",
                                                "emailuser.net",
                                                "eml.cc",
                                                "engineer.com",
                                                "englandmail.com",
                                                "europe.com",
                                                "europemail.com",
                                                "everymail.net",
                                                "everyone.net",
                                                "execs.com",
                                                "exemail.com.au",
                                                "f-m.fm",
                                                "facebook.com",
                                                "fast-email.com",
                                                "fast-mail.org",
                                                "fastem.com",
                                                "fastemail.us",
                                                "fastemailer.com",
                                                "fastest.cc",
                                                "fastimap.com",
                                                "fastmail.cn",
                                                "fastmail.co.uk",
                                                "fastmail.com.au",
                                                "fastmail.es",
                                                "fastmail.fm",
                                                "fastmail.im",
                                                "fastmail.in",
                                                "fastmail.jp",
                                                "fastmail.mx",
                                                "fastmail.net",
                                                "fastmail.nl",
                                                "fastmail.se",
                                                "fastmail.to",
                                                "fastmail.tw",
                                                "fastmail.us",
                                                "fastmailbox.net",
                                                "fastmessaging.com",
                                                "fastservice.com",
                                                "fea.st",
                                                "financier.com",
                                                "fireman.net",
                                                "flashmail.com",
                                                "fmail.co.uk",
                                                "fmailbox.com",
                                                "fmgirl.com",
                                                "fmguy.com",
                                                "ftml.net",
                                                "galaxyhit.com",
                                                "gardener.com",
                                                "geologist.com",
                                                "germanymail.com",
                                                "gmail.com",
                                                "gmx.com",
                                                "googlemail.com",
                                                "graduate.org",
                                                "graphic-designer.com",
                                                "greenmail.net",
                                                "groupmail.com",
                                                "guerillamail.com",
                                                "h-mail.us",
                                                "hackermail.com",
                                                "hailmail.net",
                                                "hairdresser.net",
                                                "hilarious.com",
                                                "hiphopfan.com",
                                                "homemail.com",
                                                "hot-shot.com",
                                                "hotmail.co.uk",
                                                "hotmail.com",
                                                "hotmail.fr",
                                                "hotmail.it",
                                                "housemail.com",
                                                "humanoid.net",
                                                "hushmail.com",
                                                "icloud.com",
                                                "iinet.net.au",
                                                "imap-mail.com",
                                                "imap.cc",
                                                "imapmail.org",
                                                "iname.com",
                                                "inbox.com",
                                                "innocent.com",
                                                "inorbit.com",
                                                "inoutbox.com",
                                                "instruction.com",
                                                "instructor.net",
                                                "insurer.com",
                                                "internet-e-mail.com",
                                                "internet-mail.org",
                                                "internetemails.net",
                                                "internetmailing.net",
                                                "internode.on.net",
                                                "iprimus.com.au",
                                                "irelandmail.com",
                                                "israelmail.com",
                                                "italymail.com",
                                                "jetemail.net",
                                                "job4u.com",
                                                "journalist.com",
                                                "justemail.net",
                                                "keromail.com",
                                                "kissfans.com",
                                                "kittymail.com",
                                                "koreamail.com",
                                                "lawyer.com",
                                                "legislator.com",
                                                "letterboxes.org",
                                                "linuxmail.org",
                                                "live.co.uk",
                                                "live.com",
                                                "live.com.au",
                                                "lobbyist.com",
                                                "lovecat.com",
                                                "lycos.com",
                                                "mac.com",
                                                "madonnafan.com",
                                                "mail-central.com",
                                                "mail-me.com",
                                                "mail-page.com",
                                                "mail.com",
                                                "mail.ru",
                                                "mailandftp.com",
                                                "mailas.com",
                                                "mailbolt.com",
                                                "mailc.net",
                                                "mailcan.com",
                                                "mailforce.net",
                                                "mailftp.com",
                                                "mailhaven.com",
                                                "mailinator.com",
                                                "mailingaddress.org",
                                                "mailite.com",
                                                "mailmight.com",
                                                "mailnew.com",
                                                "mailsent.net",
                                                "mailservice.ms",
                                                "mailup.net",
                                                "mailworks.org",
                                                "marchmail.com",
                                                "me.com",
                                                "metalfan.com",
                                                "mexicomail.com",
                                                "minister.com",
                                                "ml1.net",
                                                "mm.st",
                                                "moscowmail.com",
                                                "msn.com",
                                                "munich.com",
                                                "musician.org",
                                                "muslim.com",
                                                "myfastmail.com",
                                                "mymacmail.com",
                                                "myself.com",
                                                "net-shopping.com",
                                                "netspace.net.au",
                                                "ninfan.com",
                                                "nonpartisan.com",
                                                "nospammail.net",
                                                "null.net",
                                                "nycmail.com",
                                                "oath.com",
                                                "onebox.com",
                                                "operamail.com",
                                                "optician.com",
                                                "optusnet.com.au",
                                                "orthodontist.net",
                                                "outlook.com",
                                                "ownmail.net",
                                                "pacific-ocean.com",
                                                "pacificwest.com",
                                                "pediatrician.com",
                                                "petlover.com",
                                                "petml.com",
                                                "photographer.net",
                                                "physicist.net",
                                                "planetmail.com",
                                                "planetmail.net",
                                                "polandmail.com",
                                                "politician.com",
                                                "post.com",
                                                "postinbox.com",
                                                "postpro.net",
                                                "presidency.com",
                                                "priest.com",
                                                "programmer.net",
                                                "proinbox.com",
                                                "promessage.com",
                                                "protestant.com",
                                                "publicist.com",
                                                "qmail.com",
                                                "qq.com",
                                                "qualityservice.com",
                                                "radiologist.net",
                                                "ravemail.com",
                                                "realemail.net",
                                                "reallyfast.biz",
                                                "reallyfast.info",
                                                "realtyagent.com",
                                                "reborn.com",
                                                "rediff.com",
                                                "reggaefan.com",
                                                "registerednurses.com",
                                                "reincarnate.com",
                                                "religious.com",
                                                "repairman.com",
                                                "representative.com",
                                                "rescueteam.com",
                                                "rocketmail.com",
                                                "rocketship.com",
                                                "runbox.com",
                                                "rushpost.com",
                                                "safrica.com",
                                                "saintly.com",
                                                "salesperson.net",
                                                "samerica.com",
                                                "sanfranmail.com",
                                                "scientist.com",
                                                "scotlandmail.com",
                                                "secretary.net",
                                                "sent.as",
                                                "sent.at",
                                                "sent.com",
                                                "seznam.cz",
                                                "snakebite.com",
                                                "socialworker.net",
                                                "sociologist.com",
                                                "solution4u.com",
                                                "songwriter.net",
                                                "spainmail.com",
                                                "spamgourmet.com",
                                                "speedpost.net",
                                                "speedymail.org",
                                                "ssl-mail.com",
                                                "surgical.net",
                                                "swedenmail.com",
                                                "swift-mail.com",
                                                "swissmail.com",
                                                "teachers.org",
                                                "tech-center.com",
                                                "techie.com",
                                                "technologist.com",
                                                "telstra.com",
                                                "telstra.com.au",
                                                "the-fastest.net",
                                                "the-quickest.com",
                                                "theinternetemail.com",
                                                "theplate.com",
                                                "therapist.net",
                                                "toke.com",
                                                "toothfairy.com",
                                                "torontomail.com",
                                                "tpg.com.au",
                                                "trashmail.net",
                                                "tvstar.com",
                                                "umpire.com",
                                                "usa.com",
                                                "uymail.com",
                                                "veryfast.biz",
                                                "veryspeedy.net",
                                                "virginbroadband.com.au",
                                                "warpmail.net",
                                                "webname.com",
                                                "westnet.com.au",
                                                "windowslive.com",
                                                "worker.com",
                                                "workmail.com",
                                                "writeme.com",
                                                "xsmail.com",
                                                "xtra.co.nz",
                                                "y7mail.com",
                                                "yahoo.ae",
                                                "yahoo.at",
                                                "yahoo.be",
                                                "yahoo.ca",
                                                "yahoo.ch",
                                                "yahoo.cn",
                                                "yahoo.co.id",
                                                "yahoo.co.il",
                                                "yahoo.co.in",
                                                "yahoo.co.jp",
                                                "yahoo.co.kr",
                                                "yahoo.co.nz",
                                                "yahoo.co.th",
                                                "yahoo.co.uk",
                                                "yahoo.co.za",
                                                "yahoo.com",
                                                "yahoo.com.ar",
                                                "yahoo.com.au",
                                                "yahoo.com.br",
                                                "yahoo.com.cn",
                                                "yahoo.com.co",
                                                "yahoo.com.hk",
                                                "yahoo.com.mx",
                                                "yahoo.com.my",
                                                "yahoo.com.ph",
                                                "yahoo.com.sg",
                                                "yahoo.com.tr",
                                                "yahoo.com.tw",
                                                "yahoo.com.vn",
                                                "yahoo.cz",
                                                "yahoo.de",
                                                "yahoo.dk",
                                                "yahoo.es",
                                                "yahoo.fi",
                                                "yahoo.fr",
                                                "yahoo.gr",
                                                "yahoo.hu",
                                                "yahoo.ie",
                                                "yahoo.in",
                                                "yahoo.it",
                                                "yahoo.nl",
                                                "yahoo.no",
                                                "yahoo.pl",
                                                "yahoo.pt",
                                                "yahoo.ro",
                                                "yahoo.ru",
                                                "yahoo.se",
                                                "yandex.ru",
                                                "yepmail.net",
                                                "ymail.com",
                                                "your-mail.com",
                                                "zoho.com"});

  if (email_domain.empty()) {
    return false;
  }
  if (g_non_managed_domain_for_testing &&
      email_domain == g_non_managed_domain_for_testing) {
    return false;
  }

  return !kKnownConsumerDomains.contains(email_domain);
}

// static
bool AccountManagedStatusFinder::MayBeEnterpriseUserBasedOnEmail(
    const std::string& email) {
  return MayBeEnterpriseDomain(GetDomainFromEmail(email));
}

// static
void AccountManagedStatusFinder::SetNonEnterpriseDomainForTesting(
    const char* domain) {
  g_non_managed_domain_for_testing = domain;
}

AccountManagedStatusFinder::AccountManagedStatusFinder(
    signin::IdentityManager* identity_manager,
    const CoreAccountInfo& account,
    base::OnceClosure async_callback)
    : identity_manager_(identity_manager), account_(account) {
  if (!identity_manager_->AreRefreshTokensLoaded()) {
    // We want to make sure that `account` exists in the IdentityManager but
    // we can only that after tokens are loaded. Wait for the
    // `OnRefreshTokensLoaded()` notification.
    identity_manager_observation_.Observe(identity_manager_.get());
    callback_ = std::move(async_callback);
    return;
  }

  outcome_ = DetermineOutcome();
  if (outcome_ == Outcome::kPending) {
    // Wait until the account information becomes available.
    identity_manager_observation_.Observe(identity_manager_.get());
    callback_ = std::move(async_callback);
    // TODO(crbug.com/40243973): Add a timeout mechanism.
  }

  // Result is known synchronously, ignore `async_callback`.
}

AccountManagedStatusFinder::~AccountManagedStatusFinder() = default;

void AccountManagedStatusFinder::OnExtendedAccountInfoUpdated(
    const AccountInfo& info) {
  DCHECK_EQ(outcome_, Outcome::kPending);

  // Don't care about other accounts.
  if (info.account_id != account_.account_id) {
    return;
  }

  // Keep waiting if `info` isn't complete yet.
  if (!info.IsValid()) {
    return;
  }

  // This is the relevant account! Determine its type.
  OutcomeDeterminedAsync(info.IsManaged() ? Outcome::kEnterprise
                                          : Outcome::kConsumerNotWellKnown);
}

void AccountManagedStatusFinder::OnRefreshTokenRemovedForAccount(
    const CoreAccountId& account_id) {
  DCHECK_EQ(outcome_, Outcome::kPending);

  // Don't care about other accounts.
  if (account_id != account_.account_id) {
    return;
  }

  // The interesting account was removed, we're done here.
  OutcomeDeterminedAsync(Outcome::kError);
}

void AccountManagedStatusFinder::OnRefreshTokensLoaded() {
  DCHECK_EQ(outcome_, Outcome::kPending);

  Outcome outcome = DetermineOutcome();
  if (outcome == Outcome::kPending) {
    // There is still not enough information to determine the account managed
    // status. Keep waiting for notifications from IdentityManager.
    return;
  }

  OutcomeDeterminedAsync(outcome);
}

void AccountManagedStatusFinder::OnIdentityManagerShutdown(
    signin::IdentityManager* identity_manager) {
  DCHECK_EQ(outcome_, Outcome::kPending);

  OutcomeDeterminedAsync(Outcome::kError);
}

AccountManagedStatusFinder::Outcome
AccountManagedStatusFinder::DetermineOutcome() const {
  // This must be called only after refresh tokens have been loaded.
  CHECK(identity_manager_->AreRefreshTokensLoaded());

  // First make sure that the account actually exists in the IdentityManager,
  // then check the easy cases: For most accounts, it's possible to statically
  // tell the account type from the email.
  if (!identity_manager_->HasAccountWithRefreshToken(account_.account_id)) {
    return Outcome::kError;
  }

  if (IsGmailUserBasedOnEmail(account_.email)) {
    return Outcome::kConsumerGmail;
  }

  if (!MayBeEnterpriseUserBasedOnEmail(account_.email)) {
    return Outcome::kConsumerWellKnown;
  }

  if (gaia::IsGoogleInternalAccountEmail(
          gaia::CanonicalizeEmail(account_.email))) {
    // Special case: @google.com accounts are a particular sub-type of
    // enterprise accounts.
    return Outcome::kEnterpriseGoogleDotCom;
  }

  // The easy cases didn't apply, so actually get the canonical info from
  // IdentityManager. This may or may not be available immediately.
  AccountInfo info = identity_manager_->FindExtendedAccountInfo(account_);
  if (info.IsValid()) {
    return info.IsManaged() ? Outcome::kEnterprise
                            : Outcome::kConsumerNotWellKnown;
  }

  // Extended account info isn't (fully) available yet. Observe the
  // IdentityManager to get notified once it is.
  return Outcome::kPending;
}

void AccountManagedStatusFinder::OutcomeDeterminedAsync(Outcome type) {
  DCHECK_EQ(outcome_, Outcome::kPending);
  DCHECK_NE(type, Outcome::kPending);

  outcome_ = type;

  // The type of an account can't change, so no need to observe any longer.
  identity_manager_observation_.Reset();
  identity_manager_ = nullptr;

  // Let the client know the type was determined.
  std::move(callback_).Run();
}

}  // namespace signin
