# About //chromeos/ash/components/osauth

This directory contains components that implement OS User authentication
(local authentication, opposed to online GAIA/SAML authentication), and
management of local authentication factors.

With
[Cryptohome modernization](https://chromium.googlesource.com/chromiumos/platform2/+/HEAD/cryptohome/docs/cryptohome_modernization_apis.md)
it is now possible to decouple authentication phase from the rest of the
login logic, and turn it into component reused on Login / Lock screens, as
well as for other in-session authentication, e.g WebAuthN flow.

Eventually authentication-specific logic would be moved from
`//chromeos/ash/components/login` to this component.

See also `//chrome/browser/ash/login/osauth` for elements that depend on
browser stack (such as policies).
