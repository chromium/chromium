# //components/encrypted_messages

Do not add new uses of this component without consulting the security team.

This component implements an extremely simple cryptographic scheme, which allows
encrypting anonymous messages to a remote end with a known, static public key.
The scheme is as follows, with a server public key spub:

1. Generate a random X25519 keypair (cpub, cpriv)
2. Compute a shared secret using spub and cpriv - note that the server will be
   able to compute the same shared secret using cpub and spriv later
3. Compute a subkey from that shared secret and a client-supplied fixed label
   string (to provide domain separation) using HKDF
4. Use that subkey with an AEAD (currently always AES-128-CTR with HMAC-SHA256)
   to encrypt and authenticate the payload
5. Pack the result into a protobuf containing the message itself, a server
   public key identifier, the random cpub from step 1, and an algorithm
   identifier for the inner AEAD

This is used in situations where Chromium is forced to communicate
maybe-sensitive data over plain HTTP - namely metrics and variation requests,
which are scenarios in which HTTPS might be unavailable.

Do not add new uses of this component without consulting the security team.
