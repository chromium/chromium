# google\_update\_win.cc.patch
Debugging the legacy on-demand Windows client in Chrome requires a
patch to get rid of some DCHECKs in the code. Developers can apply the
patch `google_update_win.cc.patch` in a local branch and rebuild
Chrome with it. Don't land the patch with your changes!

# selfupdate\_test\_key.der
A DER-formatted PKCS #8 PrivateKeyInfo for an RSA key used to sign the self-
update CRX used in integration tests. You can regenerate it by running:

```
openssl genrsa 4096 | openssl pkcs8 -inform PEM -nocrypt -topk8 -outform DER \
    -out selfupdate_test_key.der
```
