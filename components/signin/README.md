This component supports cross-platform access to and mutation of the user's Gaia
identities. The core interfaces of interest to most consumers reside in
public/identity_manager; see its README.md for documentation of those
interfaces.

The complete structure of the component is as follows:

core/: Higher-level code that is publicly visible to consumers. Code therein will
be transitioned to public/ and/or internal/.
internal/: The internal implementation of public/. Not visible to consumers.
ios/: Higher-level iOS-specific code that is publicly visible to consumers. Code
therein will be transitioned to public/ and/or internal/.
public/: The long-term public API surface of the component.
