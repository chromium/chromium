Kcer is a library to work with keys and client certificates on ChromeOS.

It can be accessed through the `kcer::Kcer` interface. Each Profile owns an
instance of the interface and can use it to access keys and client certificates
that are intended for the Profile. There's also a device-wide instance of the
interface that only has access to device-wide objects.

Kcer can work with user and device tokens (which are provided by Chaps).
User token contains keys and certificates that belong to the current user.
Device token contains objects that belong to the whole device.
A particular instance of Kcer interface might not have access to one or both of
them depending on its Profile.

`kcer::PublicKey` objects contain a public key from a key pair that is available
to Kcer. It can also be used to address its private key.

`kcer::Cert` contains a client certificate for a key pair. As a memory
optimization `kcer::Cert`-s are refcounted (and immutable). `kcer::Cert`-s can
also be used to address their private key.

Private keys themselves are not exposed through Kcer and all related operations
are performed by dedicated system daemons (mainly Chaps).
`kcer::PrivateKeyHandle` is a convenience class to address a private key that
corresponds to a public key / public key SPKI (as a blob of bytes) / certificate
and request an operation with it.

See kcer.h for more details about the interface.
