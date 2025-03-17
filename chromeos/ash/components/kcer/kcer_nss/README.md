kcer_nss directory contains `KcerNss` - an implementation of `Kcer`
(see `//components/kcer`) that relies on NSS to store keys and client
certificates.

`KcerNss` is a temporary implementation for the transition period.
The plan is:
* Implement `KcerNss`.
* Refactor existing code that uses NSS to use `KcerNss`.
* Implement Kcer (that has the same interface as `KcerNss`,
but doesn't rely on NSS).
* Switch to using `Kcer` without NSS.
* Remove NSS and `KcerNss`.
