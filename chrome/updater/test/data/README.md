# selfupdate\_test\_key.der
A DER-formatted PKCS #8 PrivateKeyInfo for an RSA key used to sign the self-
update CRX used in integration tests. You can regenerate it by running:

```
openssl genrsa 4096 | openssl pkcs8 -inform PEM -nocrypt -topk8 -outform DER \
    -out selfupdate_test_key.der
```
