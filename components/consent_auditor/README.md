# Consent Auditor

The consent auditor component is a service containing methods used for
recording and retrieving the records of the exact language the user consented to
when enabling a feature.

These methods should be called from any UI which enables a feature based on
the user's consent, and record the consent language that the user has seen.
If we later find out there was a mistranslation, need to expand the scope of
a setting etc., we will know which users are affected.

TO EMPHASIZE, these methods should ALWAYS be called from the corresponding UI,
not from the backend code or pref change observers. The latter could cause
the consent to be wrongfully recorded if feature was enabled through other
means (through extensions, a new UI surface added, or even a bug).

Consents related to features for signed-in users will be stored in the user's
Google account.
